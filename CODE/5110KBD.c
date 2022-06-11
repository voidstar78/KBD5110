/*

Tested using Arduino Nano serial connection at 38400 baud
with IBM 5110 Type 2 (BASIC-only, no internal tape)

NOTE: I had to use "old bootloader" to upload this "Scratch" to the specific Nano that I was using (from the Arduino IDE).

You may be able to increase the Port speed, initially I was using 115200.  But during bulk uploads of 
buffered text data (i.e. paste into terminal), sometimes issues arose where characters were getting lost.
I tried also 9600, but decided to "standardize" all timing around 38400.

The IBM 5100's have several keys not represented in standard ASCII, such as ATTN, Arrow Keys, HOLD, etc.
To support these in a serial connection, I use a "parse_key" buffer and use the "^" (caret) symbol has a START/STOP
token to indicate when a parse_key is being indicated.  By convention, I kept each code to 2-characters only.

I later realized some of these special keys could be specified using CTRL codes (ASCII 1 to ASCII 26).
For example, CTRL-H is typically backspace and CTRL-M is typically enter.

(on the IBM 5110, "CMD" is their version of CTRL)

ASCII                         IBM 5110 Key
CTRL-A                        UP ARROW
CTRL-Z                        DOWN ARROW
CTRL-P                        RIGHT ARROW
CTRL-O                        LEFT ARROW

CTRL-L                        HOLD
CTRL-M                        EXECUTE

CTRL-R                        CMD-ATTN               [I tried to make "~" also be CMD-ATTN, but it doesn't seem to work]
CTRL-T                        CMD-MULTIPLY (STAR)
CTRL-G                        CMD-MINUS
CTRL-B                        CMD-PLUS

ESCAPE                        ATTN

Most other keys (A-Z, 0-9, $&*-+;:.,() should convert as expected)

NOTE: To enter lower case mode on the IBM 5110 -- press HOLD, then SHIFT-DOWN.

Parsed keys offer an alternative way to express CTRL-x commands (such as if your system
doesn't support CTRL or can't express non-printable characters).  Parsed keys also
can adjust modes or perform actions (like to DELAY a certain amount of time, to allow
the IBM 5110 system to finish processing a prior command and avoid "typing too fast").

PARSED KEYS
-----------
^LE^                          LEFT ARROW
^RI^                          RIGHT ARROW
^UP^                          UP ARROW
^DO^                          DOWN ARROW

^SU^                          SHIFT-UP ARROW
^SD^                          SHIFT-DOWN ARROW

^HO^                          HOLD
^EX^                          EXECUTE
^AT^                          ATTN

^CA^                          CMD-ATTN
^CP^                          CMD-PLUS
^CM^                          CMD-MINUS
^CS^                          CMD-MULITPLY (STAR)

^Dx^                          DELAY (x = 1 to 9, delay x * 100 milliseconds, e.g. ^D3^ delays 300ms)

^E0^                          TURN OFF INVOKING EXECUTE KEY (used when scripting text files that contain CRLF at end of line)
^E1^                          TURN ON INVOKING EXECUTE KEY

*/

// These are the expected sequence between the Arduino digital outputs and the green KEYBOARD header on the IBM 5110.
// A 330ohm resistor is placed inline between these pinouts and the IBM 5110 keyboard connector.
//   IBM Kbd Connector                Arduino
//   KBD_P --------\/\/330ohm\/\/\---- D02
//   KBD_7 --------\/\/330ohm\/\/\---- D03
//   KBD_6 --------\/\/330ohm\/\/\---- D04
//   KBD_5 --------\/\/330ohm\/\/\---- D05
//   KBD_4 --------\/\/330ohm\/\/\---- D06
//   KBD_3 --------\/\/330ohm\/\/\---- D07
//   KBD_2 --------\/\/330ohm\/\/\---- D08
//   KBD_1 --------\/\/330ohm\/\/\---- D09
//   KBD_0 --------\/\/330ohm\/\/\---- D10
//   KBD_STROBE----\/\/330ohm\/\/\---- D11

#define PIN_KBD_P       2

#define PIN_KBD_7       3   // KBD_7
#define PIN_KBD_6       4   // KBD_6
#define PIN_KBD_5       5   // KBD_5
#define PIN_KBD_4       6   // KBD_4
#define PIN_KBD_3       7   // KBD_3
#define PIN_KBD_2       8   // KBD_2
#define PIN_KBD_1       9   // KBD_1
#define PIN_KBD_0       10  // KBD_0

#define PIN_KBD_STROBE  11

#define TRUE 1
#define FALSE 0

// Scan code for the EXECUTE key has some special handling, so it is given
// its own specific macro definition.
#define KEY_EXECUTE 0xB2

// The table below is specific to the IBM 5110 scan codes.
// A similar table could be prepared for the IBM 5100, using the hex values in the IBM 5100 MIM.

// This table should normally have 256 entries, and indexed like the standard ASCII table.  However during
// testing, it was found two additional entries had to be artifically added to make the indexing work properly.

// NOTE: To support APL keys, we'll need to define uses of the Extended ASCII values above 128.  Or we could use "parsed_keys",
// but then a special terminal software will be need to make typing thos parsed_keys easlier.
static const int ascii_to_5110[] = {
0x00  , //  0   00  NUL        
0xDF  , //  1   01  SOH    ^A   --> UP ARROW
0x91  , //  2   02  STX    ^B   --> CMD+PLUS 
0x00  , //  3   03  ETX    ^C 
0x00  , //  4   04  EOT    ^D   
0x00  , //  5   05  ENQ    ^E   
0x00  , //  6   06  ACK    ^F   
0x93  , //  7   07  BEL    ^G  --> CMD-MINUS 
0x34  , //  8   08  BS     ^H  --> LEFT ARROW (like backspace)   
0x00  , //  9   09  HT     ^I   
0x00  , //  10  0A  LF     ^J   
0x00  , //  11  0B  VT     ^K   
0x36  , //  12  0C  FF     ^L   --> HOLD  
KEY_EXECUTE  , //  13  0D  CR ^M    EXECUTE 
0x00  , //  14  0E  SO     ^N
0x34  , //  15  0F  SI     ^O  --> LEFT ARROW
0xB4  , //  16  10  DLE    ^P  --> RIGHT ARROW
0x00  , //  17  11  DC1    ^Q 
0x96  , //  18  12  DC2    ^R  --> CMD-ATTN
0x00  , //  19  13  DC3    ^S 
0x95  , //  20  14  DC4    ^T  --> CMD-STAR 
0x00  , //  21  15  NAK    ^U 
0x00  , //  22  16  SYN    ^V 
0x00  , //  23  17  ETB    ^W 
0x00  , //  24  18  CAN    ^X 
0x00  , //  25  19  EM     ^Y 
0x4F  , //  26  1A  SUB    ^Z --> DOWN ARROW
0xB6  , //  27  1B  ESC       --> ATTN    
0x00  , //  28  1C  FS        
0x00  , //  29  1D  GS        
0x00  , //  30  1E  RS        
0x00  , //  31  1F  US        
0x39  , //  32  20  space       
0x00  , //  33  21  !   composition of SHIFT-K, backspace(left arrow), then "." (period)   [ yes, 3 keys to make 1 ! ] [ reverse order works too, "." back then SHIFT-K ] 
0x4C  , //  34  22  "   SHIFT+1   
0x30  , //  35  23  #       
0x4B  , //  36  24  $       
0x00  , //  37  25  %       
0x4A  , //  38  26  &   SHIFT+$   
0xFA  , //  39  27  '   SHIFT+K   
0x3A  , //  40  28  (       
0xBA  , //  41  29  )       
0x9D  , //  42  2A  *   KEYPAD *    
0x99  , //  43  2B  +   KEYPAD +    
0xF9  , //  44  2C  ,       
0x9B  , //  45  2D  -   KEYPAD -    
0x89  , //  46  2E  .       
0x00  , //  47  2F  /       
0x8F  , //  48  30  0   0x8E  ^ 
0x4D  , //  49  31  1   0x4C  " 
0x0F  , //  50  32  2   0x0E  - 
0xCF  , //  51  33  3   0xCE    
0xAF  , //  52  34  4   0xAE  <=  
0x2F  , //  53  35  5   0x2E    
0xEF  , //  54  36  6   0xEE  >=  
0x6F  , //  55  37  7   0x6E  \ 
0x00  , // no idea why this extra space in the table is necessary... (bug in compiler array indexing?)
0x7F  , //  56  38  8   0x7E  not equal !=  
0xFF  , //  57  39  9   0xFE  v  (not letter-V) 
0x88  , //  58  3A  :   SHIFT+.   
0xF8  , //  59  3B  ;   SHIFT+,   
0xCE  , //  60  3C  <       
0x32  , //  61  3D  =       
0x6E  , //  62  3E  >       
0x0C  , //  63  3F  ?   SHIFT+Q   
0x70  , //  64  40  @   SHIFT+=   
0x0B  , //  65  41  A   0x0C  ? 
0xE9  , //  66  42  B   0xE8  : 
0xA9  , //  67  43  C       
0xAB  , //  68  44  D       
0xAD  , //  69  45  E       
0x2B  , //  70  46  F       
0xEB  , //  71  47  G       
0x6B  , //  72  48  H       
0xFD  , //  73  49  I       
0x7B  , //  74  4A  J       
0xFB  , //  75  4B  K       
0x8B  , //  76  4C  L       
0x79  , //  77  4D  M       
0x69  , //  78  4E  N       
0x8D  , //  79  4F  O       
0x3D  , //  80  50  P       
0x0D  , //  81  51  Q   0C  ? 
0x2D  , //  82  52  R   2C    
0xCB  , //  83  53  S       
0xED  , //  84  54  T       
0x7D  , //  85  55  U       
0x29  , //  86  56  V       
0xCD  , //  87  57  W       
0xC9  , //  88  58  X       
0x6D  , //  89  59  Y       
0x09  , //  90  5A  Z       
0x00  , //  91  5B  [       
0x00  , //  92  5C  \       
0x00  , //  93  5D  ]       
0x00  , //  94  5E  ^       
0x00  , //  95  5F  _       
0x00  , //  96  60  `       
0x00  , // no idea why this extra space in the table is necessary... (bug in compiler array indexing?)
0x0B  , //  97  61  a       
0xE9  , //  98  62  b       
0xA9  , //  99  63  c       
0xAB  , //  100 64  d       
0xAD  , //  101 65  e       
0x2B  , //  102 66  f       
0xEB  , //  103 67  g       
0x6B  , //  104 68  h       
0xFD  , //  105 69  i       
0x7B  , //  106 6A  j       
0xFB  , //  107 6B  k       
0x8B  , //  108 6C  l       
0x79  , //  109 6D  m       
0x69  , //  110 6E  n       
0x8D  , //  111 6F  o       
0x3D  , //  112 70  p       
0x0D  , //  113 71  q       
0x2D  , //  114 72  r       
0xCB  , //  115 73  s       
0xED  , //  116 74  t       
0x7D  , //  117 75  u       
0x29  , //  118 76  v       
0xCD  , //  119 77  w       
0xC9  , //  120 78  x       
0x6D  , //  121 79  y       
0x09  , //  122 7A  z       
0x00  , //  123 7B  {       
0x00  , //  124 7C  |       
0x00  , //  125 7D  }       
0x96  , //  126 7E  ~    -->   CMD+ATTN    
0x34  , //  127 7F  DEL       
0x00  , //  128 80           
0x00  , //  129 81           
0x00  , //  130 82           
0x00  , //  131 83           
0x00  , //  132 84           
0x00  , //  133 85           
0x00  , //  134 86           
0x00  , //  135 87           
0x00  , //  136 88           
0x00  , //  137 89           
0x00  , //  138 8A           
0x00  , //  139 8B           
0x00  , //  140 8C           
0x00  , //  141 8D           
0x00  , //  142 8E           
0x00  , //  143 8F           
0x00  , //  144 90           
0x00  , //  145 91           
0x00  , //  146 92           
0x00  , //  147 93           
0x00  , //  148 94           
0x00  , //  149 95           
0x00  , //  150 96           
0x00  , //  151 97           
0x00  , //  152 98           
0x00  , //  153 99           
0x00  , //  154 9A           
0x00  , //  155 9B           
0x00  , //  156 9C           
0x00  , //  157 9D           
0x00  , //  158 9E           
0x00  , //  159 9F           
0x00  , //  160 A0            
0x00  , //  161 A1  ¡         
0x00  , //  162 A2  ¢         
0x00  , //  163 A3  £         
0x00  , //  164 A4  ¤         
0x00  , //  165 A5  ¥         
0x00  , //  166 A6  ¦         
0x00  , //  167 A7  §         
0x00  , //  168 A8  ¨         
0x00  , //  169 A9  ©         
0x00  , //  170 AA  ª         
0x00  , //  171 AB  «         
0x00  , //  172 AC  ¬         
0x00  , //  173 AD  ­         
0x00  , //  174 AE  ®         
0x00  , //  175 AF  ¯         
0x00  , //  176 B0  °         
0x00  , //  177 B1  ±         
0x00  , //  178 B2  ²         
0x00  , //  179 B3  ³         
0x00  , //  180 B4  ´         
0x00  , //  181 B5  µ         
0x00  , //  182 B6  ¶         
0x00  , //  183 B7  ·         
0x00  , //  184 B8  ¸         
0x00  , //  185 B9  ¹         
0x00  , //  186 BA  º         
0x00  , //  187 BB  »         
0x00  , //  188 BC  ¼         
0x00  , //  189 BD  ½         
0x00  , //  190 BE  ¾         
0x00  , //  191 BF  ¿         
0x00  , //  192 C0  À         
0x00  , //  193 C1  Á         
0x00  , //  194 C2  Â         
0x00  , //  195 C3  Ã         
0x00  , //  196 C4  Ä         
0x00  , //  197 C5  Å         
0x00  , //  198 C6  Æ         
0x00  , //  199 C7  Ç         
0x00  , //  200 C8  È         
0x00  , //  201 C9  É         
0x00  , //  202 CA  Ê         
0x00  , //  203 CB  Ë         
0x00  , //  204 CC  Ì         
0x00  , //  205 CD  Í         
0x00  , //  206 CE  Î         
0x00  , //  207 CF  Ï         
0x00  , //  208 D0  Ð         
0x00  , //  209 D1  Ñ         
0x00  , //  210 D2  Ò         
0x00  , //  211 D3  Ó         
0x00  , //  212 D4  Ô         
0x00  , //  213 D5  Õ         
0x00  , //  214 D6  Ö         
0x00  , //  215 D7  ×         
0x00  , //  216 D8  Ø         
0x00  , //  217 D9  Ù         
0x00  , //  218 DA  Ú         
0x00  , //  219 DB  Û         
0x00  , //  220 DC  Ü         
0x00  , //  221 DD  Ý         
0x00  , //  222 DE  Þ         
0x00  , //  223 DF  ß         
0x00  , //  224 E0  à         
0x00  , //  225 E1  á         
0x00  , //  226 E2  â         
0x00  , //  227 E3  ã         
0x00  , //  228 E4  ä         
0x00  , //  229 E5  å         
0x00  , //  230 E6  æ         
0x00  , //  231 E7  ç         
0x00  , //  232 E8  è         
0x00  , //  233 E9  é         
0x00  , //  234 EA  ê         
0x00  , //  235 EB  ë         
0x00  , //  236 EC  ì         
0x00  , //  237 ED  í         
0x00  , //  238 EE  î         
0x00  , //  239 EF  ï         
0x00  , //  240 F0  ð         
0x00  , //  241 F1  ñ         
0x00  , //  242 F2  ò         
0x00  , //  243 F3  ó         
0x00  , //  244 F4  ô         
0x00  , //  245 F5  õ         
0x00  , //  246 F6  ö         
0x00  , //  247 F7  ÷         
0x00  , //  248 F8  ø         
0x00  , //  249 F9  ù         
0x00  , //  250 FA  ú         
0x00  , //  251 FB  û         
0x00  , //  252 FC  ü         
0x00  , //  253 FD  ý         
0x00  , //  254 FE  þ         
0x00  , //  255 FF  ÿ       
};

static int parity_values[256];

int incomingByte = 0;    // assuming 16-bit int, so bit 8 is not sign bit  [ could/should probably use "byte" type ]
int out_scan_code = -1;
int out_parity = -1;
int parse_key_mode = FALSE;
int interpret_crlf_as_execute = TRUE;  // toggled using E0 E1 parsed_key

void setup() {
  
  pinMode(PIN_KBD_P, INPUT);
  pinMode(PIN_KBD_0, INPUT);
  pinMode(PIN_KBD_1, INPUT);
  pinMode(PIN_KBD_2, INPUT);
  pinMode(PIN_KBD_3, INPUT);
  pinMode(PIN_KBD_4, INPUT);
  pinMode(PIN_KBD_5, INPUT);
  pinMode(PIN_KBD_6, INPUT);
  pinMode(PIN_KBD_7, INPUT);
  pinMode(PIN_KBD_STROBE, INPUT);

  digitalWrite(PIN_KBD_P, LOW);
  digitalWrite(PIN_KBD_0, LOW);
  digitalWrite(PIN_KBD_1, LOW);
  digitalWrite(PIN_KBD_2, LOW);
  digitalWrite(PIN_KBD_3, LOW);
  digitalWrite(PIN_KBD_4, LOW);
  digitalWrite(PIN_KBD_5, LOW);
  digitalWrite(PIN_KBD_6, LOW);
  digitalWrite(PIN_KBD_7, LOW);
  digitalWrite(PIN_KBD_STROBE, LOW);  

//  Serial.begin(9600);
  Serial.begin(38400);
//  Serial.begin(57600);
//  Serial.begin(115200);  // works for slower "interactive mode", but had dropouts during pasted-text
//  Serial.begin(500000);
  while (!Serial)
  {
    // do nothing...
  }
  Serial.println("Serial connection established!");

  // Initialize all the KBD_PARITY bits or the given set of scan codes
  // This is done in-code in case the scan codes change (e.g. 5100 vs 5110), and also so that they are
  // just computed once instead of during each key press.  The parity bit for the 5110 is just a count of the number
	// of bits in the scancode (ODD is TRUE, EVEN is FALSE).   If the IBM 5110 does not receive the expected
	// parity signal for the corresponding scan code, the system will stop/freeze/lockup (a RESET or power cycle is then required).
  int i = 0;
  while (i < 256)
  {
    int temp_value = ascii_to_5110[i];  // get 5110 scan_code associated with this ASCII index
    int bit_count = 0;
    parity_values[i] = FALSE;  // assume EVEN parity
    if ((temp_value & 0x80) == 0x80) ++bit_count;
    if ((temp_value & 0x40) == 0x40) ++bit_count;
    if ((temp_value & 0x20) == 0x20) ++bit_count;
    if ((temp_value & 0x10) == 0x10) ++bit_count;
    if ((temp_value & 0x08) == 0x08) ++bit_count;
    if ((temp_value & 0x04) == 0x04) ++bit_count;
    if ((temp_value & 0x02) == 0x02) ++bit_count;
    if ((temp_value & 0x01) == 0x01) ++bit_count;
    if ((bit_count % 2) != 0) parity_values[i] = TRUE;  // if ODD parity, set parity_value for this index to be TRUE
    ++i;
  }

}

#define MAX_PARSE_KEY_BUFFER_LENGTH 100
char parse_key_buffer[MAX_PARSE_KEY_BUFFER_LENGTH];
int parse_key_buffer_index = -1;

// Arduino Nano has an internal buffer of 64 bytes.
/*
Duplicating this buffer ended up not being necessary...
#define MAX_INPUT_BYTES_AT_A_TIME 512
byte receivedBytes[MAX_INPUT_BYTES_AT_A_TIME];
byte numBytesReceived = 0;
*/

void loop() {

  // Assume no input...
  out_scan_code = -1;
  out_parity = -1;

  //numBytesReceived = 0;
  while (Serial.available() > 0) 
  {
    incomingByte = Serial.read();
/******************************************
    receivedBytes[numBytesReceived++] = incomingByte;
    if (incomingByte == '^')
    {
      break;
    }
    if (numBytesReceived >= MAX_INPUT_BYTES_AT_A_TIME)
    {
      break;
    }
  }
//  if (numBytesReceived > 0) Serial.println(numBytesReceived);

  // NOTE: The serial buffer (32 or 128 bytes, depends on Arduino configuration)

  for (int i = 0; i < numBytesReceived; ++i)
  {
    incomingByte = receivedBytes[i];
**************/

//    Serial.print(incomingByte, DEC);
//    Serial.print(" -> ");

    if (parse_key_mode == TRUE)
    {
//      Serial.print("parse_key_buffer[");
//      Serial.print(parse_key_buffer_index);
//      Serial.print("] = ");
//      Serial.println(incomingByte, HEX);
      
      parse_key_buffer[parse_key_buffer_index] = incomingByte;
      ++parse_key_buffer_index;
      if (parse_key_buffer_index >= MAX_PARSE_KEY_BUFFER_LENGTH)  // avoid some spammed invalid parse_key
      {
        --parse_key_buffer_index;
      }
      out_scan_code = 0x00;
    }
    else
    {
      out_scan_code = ascii_to_5110[incomingByte];  // index the ASCII table by incoming ASCII byte value, to get the mapped IBM 5110 scan code to use in response
      out_parity = parity_values[incomingByte];  // relative to same index, the expected parity bit for each scan code was pre-computed at startup

      if (out_scan_code == KEY_EXECUTE)
      {
				// However we are command to issue an EXECUTE (scan code or parsed_key), double check whether we are "authorized"
				// or configured to actually send out EXECUTE commands right now.
				// (during certain scripted inputs, we may want to disable doing this, so that the scripted input can maintain its original format for convenience)
        if (interpret_crlf_as_execute == FALSE)
        {
          out_scan_code = -1;
          out_parity = -1;
        }
      }
    }
    
    if (out_scan_code == 0x00)  // scan code translation gets priority, but a zero scan value means no translation to the given ASCII was specified..
    {
			// So check if we are STARTING or ENDING a parsed_key...
      if (incomingByte == '^')  // '^'
      {
        if (parse_key_mode == TRUE)
        {
          // already in PARSE_KEY_MODE, so exit this mode and parse the buffered parse_key
          parse_key_mode = FALSE;
          parse_key_buffer_index = -1;

//          Serial.println("Interpreting special key...");

          // interpret the buffered parse_key
               if ((parse_key_buffer[0] == 'L') && (parse_key_buffer[1] == 'E')) out_scan_code = 0x34;  // LEFT ARROW
          else if ((parse_key_buffer[0] == 'R') && (parse_key_buffer[1] == 'I')) out_scan_code = 0xB4;  // RIGHT ARROW
          else if ((parse_key_buffer[0] == 'U') && (parse_key_buffer[1] == 'P')) out_scan_code = 0xDF;  // UP ARROW
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == 'O')) out_scan_code = 0x4F;  // DOWN ARROW
          
          else if ((parse_key_buffer[0] == 'S') && (parse_key_buffer[1] == 'U')) out_scan_code = 0xDE;  // SHIFT-DOWN ARROW
					else if ((parse_key_buffer[0] == 'S') && (parse_key_buffer[1] == 'D')) out_scan_code = 0x4E;  // SHIFT-DOWN ARROW

          else if ((parse_key_buffer[0] == 'H') && (parse_key_buffer[1] == 'O')) out_scan_code = 0x36;  // HOLD
          else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == 'X')) out_scan_code = 0xB2;  // EXECUTE
          else if ((parse_key_buffer[0] == 'A') && (parse_key_buffer[1] == 'T')) out_scan_code = 0xB6;  // ATTN
          
          else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'A')) out_scan_code = 0x96;  // CMD+ATTN
          else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'P')) out_scan_code = 0x91;  // CMD+PLUS
          else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'M')) out_scan_code = 0x93;  // CMD+MINUS
          else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'S')) out_scan_code = 0x95;  // CMD+STAR (multiply)
          
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '1')) { delay(100); out_scan_code = -1; }  // DELAY 1
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '2')) { delay(200); out_scan_code = -1; }  // DELAY 2
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '3')) { delay(300); out_scan_code = -1; }  // DELAY 3
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '4')) { delay(400); out_scan_code = -1; }  // DELAY 4
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '5')) { delay(500); out_scan_code = -1; }  // DELAY 5
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '6')) { delay(600); out_scan_code = -1; }  // DELAY 6
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '7')) { delay(700); out_scan_code = -1; }  // DELAY 7
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '8')) { delay(800); out_scan_code = -1; }  // DELAY 8
          else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '9')) { delay(900); out_scan_code = -1; }  // DELAY 9

          else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == '0')) { interpret_crlf_as_execute = FALSE; }  // turn OFF CRLF interpretation
          else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == '1')) { interpret_crlf_as_execute = TRUE; }  // turn ON CRLF interpretation (default)

          else                                                                 out_scan_code = -1;  // NOTE: this case means parsed_key can be used as comments by just specifying an invalid code, e.g. ^XX comment^, that won't get translated into any inputs/keys

          // Because the parsed key may translate a scan_code not in the original ascii_to_XXX table (especially if CMD or SHIFT are involved), we must
					// manually determine the PARITY bit value on the fly.
          if (out_scan_code >= 0)
          {
            // initialize out_parity for this scan code
            int bit_count = 0;

            out_parity = FALSE;  // assume EVEN parity
            if ((out_scan_code & 0x80) == 0x80) ++bit_count;
            if ((out_scan_code & 0x40) == 0x40) ++bit_count;
            if ((out_scan_code & 0x20) == 0x20) ++bit_count;
            if ((out_scan_code & 0x10) == 0x10) ++bit_count;
            if ((out_scan_code & 0x08) == 0x08) ++bit_count;
            if ((out_scan_code & 0x04) == 0x04) ++bit_count;
            if ((out_scan_code & 0x02) == 0x02) ++bit_count;
            if ((out_scan_code & 0x01) == 0x01) ++bit_count;
            if ((bit_count % 2) != 0) out_parity = TRUE;  // if ODD parity, set parity_value for this index to be TRUE  
          }
        }
        else  // START/ENTER parse_key mode...
        {
  //        Serial.println("Start PARSE_KEY mode...");
          parse_key_mode = TRUE;
          parse_key_buffer_index = 0;  // reset back to the beginning of the buffer
        
          out_scan_code = -1;
          out_parity = -1;
        }
      }
      else  // not the parsed_key token, so just ignore the ASCII input...
      {
        out_scan_code = -1;
        out_parity = -1;
      }
    }

    if (out_scan_code >= 0)  // some ASCII or parse_key translation was determined...
    {
 //     Serial.print("OUT: ");
 //     Serial.print(out_scan_code, BIN);
 //     Serial.print(" ");
 //     Serial.println(out_parity, DEC);

      // Pull "down" whichever bits in the scan code are 0's...
      if ((out_scan_code & 0x80) == 0x00) pinMode(PIN_KBD_0, OUTPUT);  
      if ((out_scan_code & 0x40) == 0x00) pinMode(PIN_KBD_1, OUTPUT);  
      if ((out_scan_code & 0x20) == 0x00) pinMode(PIN_KBD_2, OUTPUT);
      if ((out_scan_code & 0x10) == 0x00) pinMode(PIN_KBD_3, OUTPUT);      
      if ((out_scan_code & 0x08) == 0x00) pinMode(PIN_KBD_4, OUTPUT);
      if ((out_scan_code & 0x04) == 0x00) pinMode(PIN_KBD_5, OUTPUT);
      if ((out_scan_code & 0x02) == 0x00) pinMode(PIN_KBD_6, OUTPUT);
      if ((out_scan_code & 0x01) == 0x00) pinMode(PIN_KBD_7, OUTPUT);      
      if (out_parity == 0)                pinMode(PIN_KBD_P, OUTPUT); 

      pinMode(PIN_KBD_STROBE, OUTPUT);  // trigger ON  the STROBE for the scancode being pressed
      delay(10);  // oscope on 5110 observed 60ms between repeat keys; but 10ms works here (0-4ms did not work for me)
      pinMode(PIN_KBD_STROBE, INPUT);   // trigger OFF the STROBE

      // revert back whatever was "pulled down"
      if ((out_scan_code & 0x80) == 0x00) pinMode(PIN_KBD_0, INPUT);
      if ((out_scan_code & 0x40) == 0x00) pinMode(PIN_KBD_1, INPUT);
      if ((out_scan_code & 0x20) == 0x00) pinMode(PIN_KBD_2, INPUT);
      if ((out_scan_code & 0x10) == 0x00) pinMode(PIN_KBD_3, INPUT);
      if ((out_scan_code & 0x08) == 0x00) pinMode(PIN_KBD_4, INPUT);
      if ((out_scan_code & 0x04) == 0x00) pinMode(PIN_KBD_5, INPUT);
      if ((out_scan_code & 0x02) == 0x00) pinMode(PIN_KBD_6, INPUT);
      if ((out_scan_code & 0x01) == 0x00) pinMode(PIN_KBD_7, INPUT);
      if (out_parity == FALSE)            pinMode(PIN_KBD_P, INPUT);
    
    }
    else  
    {
			// could not determine how to interpret the given ASCII data... not an error, just nothing to do.
    }
  }  // end while (Serial.available() > 0) 
}
