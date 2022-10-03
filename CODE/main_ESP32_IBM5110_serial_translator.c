/*
Compiled and uploaded to ESP32 10/1/2022
Verified working with IBM 5110 Type 2

The Arduino version is in 5110KBD.c
See the notes at the top of that, the usage is identical.

   IBM kbd Connector                  ESP32
                                    (USB connector on this side)
   B04/KBD_P --------\/\/330ohm\/\/\---- D0   0
   B12/KBD_7 --------\/\/330ohm\/\/\---- D15  15
   B13/KBD_6 --------\/\/330ohm\/\/\---- D2   2
   B10/KBD_5 --------\/\/330ohm\/\/\---- D4   4
   B09/KBD_4 --------\/\/330ohm\/\/\---- RX2  16
   B08/KBD_3 --------\/\/330ohm\/\/\---- TX2  17
   D13/KBD_2 --------\/\/330ohm\/\/\---- D5   5 
   D06/KBD_1 --------\/\/330ohm\/\/\---- D18  18
   B05/KBD_0 --------\/\/330ohm\/\/\---- D19  19
   B07/KBD_STROBE----\/\/330ohm\/\/\---- D21  21
                                   (Wireless chip towards this side)

*/
#include "freertos/FreeRTOS.h"   
#include "freertos/task.h"  // vTaskDelay
#include "driver/gpio.h"  // gpio_XXX functions

#define MAX_PARSE_KEY_BUFFER_LENGTH 100
char parse_key_buffer[MAX_PARSE_KEY_BUFFER_LENGTH];
int parse_key_buffer_index = -1;

//               verified      ESP32 board label            Arduino pin#
#define PIN_KBD_P       0   // D0                           2

#define PIN_KBD_7       15  // D15                          3
#define PIN_KBD_6       2   // D2                           4
#define PIN_KBD_5       4   // D4                           5
#define PIN_KBD_4       16  // RX2                          6
#define PIN_KBD_3       17  // TX2                          7
#define PIN_KBD_2       5   // D5                           8
#define PIN_KBD_1       18  // D18                          9 
#define PIN_KBD_0       19  // D19                          10

#define PIN_KBD_STROBE  21  // D21                          11

#define TRUE 1
#define FALSE 0

#define LOW 0
#define HIGH 1

#define KEY_EXECUTE 0xB2

static const int ascii_to_5110[] = {
// These hex values are the scancodes used by the IBM 5110
// keyboard, as indicated in the "NO-SHIFT" row of the
// MIM manual (page 54, 250 KEY CODES, section 2-36).
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
KEY_EXECUTE  , //  10  0A  LF     ^J   (VSCODE environment is translating as NEWLINE 10 instead of CARRIAGE RETURN 13)
0x00  , //  11  0B  VT     ^K   
0x36  , //  12  0C  FF     ^L   --> HOLD  
KEY_EXECUTE  , //  13  0D  CR     ^M    EXECUTE  (the Ardunino environment translated ENTER as CARRIAGE RETURN 13)
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
0x00  , //  33  21  !   composition of ' backspace .    
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
0x6F  , //  55  37  7   0x6E  \    backslash
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
0x00  , //  92  5C  \   BACKSLASH
0x00  , //  93  5D  ]       
0x00  , //  94  5E  ^       
0x00  , //  95  5F  _       
0x00  , //  96  60  ` start single quote |
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

int incomingByte = 0;       // what code is coming in from the terminal
int out_scan_code = -1;     // translated scancode/keycode to send out (maybe 1:1 conversion, or a synthetic output based on interpreted sequence of inputs)
int out_parity = -1;        // IBM 5100 requires a valid parity value based on the content of the scancode to be output
int parse_key_mode = FALSE; // after typing "^" we enter a parse mode, that is buffered up until we encounter "^" again

// When pasting code or content, sometimes you really want to enter hex 13 or 10, without it being
// interpreted as ^M and becoming a press of EXECUTE on the IBM 5100.  ^E0^ can be used to turn off
// that translation, then later turn it back using ^E1^
int interpret_crlf_as_execute = TRUE;  

static void configure_pins(void)
{
  gpio_reset_pin(PIN_KBD_P);
  gpio_reset_pin(PIN_KBD_0);
  gpio_reset_pin(PIN_KBD_1);
  gpio_reset_pin(PIN_KBD_2);
  gpio_reset_pin(PIN_KBD_3);
  gpio_reset_pin(PIN_KBD_4);
  gpio_reset_pin(PIN_KBD_5);
  gpio_reset_pin(PIN_KBD_6);
  gpio_reset_pin(PIN_KBD_7);
  gpio_reset_pin(PIN_KBD_STROBE);

  gpio_set_direction(PIN_KBD_P, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_0, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_1, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_2, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_3, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_4, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_5, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_6, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_7, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_KBD_STROBE, GPIO_MODE_INPUT);

  gpio_set_level(PIN_KBD_P, LOW);
  gpio_set_level(PIN_KBD_0, LOW);
  gpio_set_level(PIN_KBD_1, LOW);
  gpio_set_level(PIN_KBD_2, LOW);
  gpio_set_level(PIN_KBD_3, LOW);
  gpio_set_level(PIN_KBD_4, LOW);
  gpio_set_level(PIN_KBD_5, LOW);
  gpio_set_level(PIN_KBD_6, LOW);
  gpio_set_level(PIN_KBD_7, LOW);
  gpio_set_level(PIN_KBD_STROBE, LOW); 
}

void app_main(void)
{
    configure_pins();

    // Initialize all the KBD_PARITY bits or the given set of scan codes
    // NOTE: This is done in code in case the scan codes change (e.g. 5100 vs 5110), and also so that they are
    // just computed once instead of during each key press.
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

    printf("HOST-TO-IBM5110 KEY TRANSLATION BEGIN\n");
    
    // BEGIN MAIN LOOP EXECUTIVE...
    while (1) 
    {
        // Assume no input...
        out_scan_code = -1;
        out_parity = -1;

        incomingByte = getc(stdin);  // should return EOF if nothing is pending
        if (incomingByte != EOF) 
        {
            //printf("getc = [%c|%d]\n", incomingByte, incomingByte);

            if (parse_key_mode == TRUE)
            {
                parse_key_buffer[parse_key_buffer_index] = incomingByte;
                ++parse_key_buffer_index;
                if (parse_key_buffer_index >= MAX_PARSE_KEY_BUFFER_LENGTH)
                {
                    --parse_key_buffer_index;
                }
                out_scan_code = 0x00;
            }
            else
            {
                out_scan_code = ascii_to_5110[incomingByte];
                out_parity = parity_values[incomingByte];

                if (out_scan_code == KEY_EXECUTE)
                {
                    if (interpret_crlf_as_execute == FALSE)
                    {
                        out_scan_code = -1;
                        out_parity = -1;
                    }
                }
            }
            
            if (out_scan_code == 0x00)
            {
                if (incomingByte == '^')  // '^'
                {
                    if (parse_key_mode == TRUE)
                    {
                        // already in PARSE_KEY_MODE, so exit this mode and parse the buffered parse_key
                        parse_key_mode = FALSE;
                        parse_key_buffer_index = -1;

                        // interpret the buffered parse_key
                             if ((parse_key_buffer[0] == 'L') && (parse_key_buffer[1] == 'E')) out_scan_code = 0x34;  // LEFT ARROW
                        else if ((parse_key_buffer[0] == 'R') && (parse_key_buffer[1] == 'I')) out_scan_code = 0xB4;  // RIGHT ARROW
                        else if ((parse_key_buffer[0] == 'U') && (parse_key_buffer[1] == 'P')) out_scan_code = 0xDF;  // UP ARROW
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == 'O')) out_scan_code = 0x4F;  // DOWN ARROW
                        
                        else if ((parse_key_buffer[0] == 'H') && (parse_key_buffer[1] == 'O')) out_scan_code = 0x36;  // HOLD
                        else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == 'X')) out_scan_code = 0xB2;  // EXECUTE
                        else if ((parse_key_buffer[0] == 'A') && (parse_key_buffer[1] == 'T')) out_scan_code = 0xB6;  // ATTN
                        
                        else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'A')) out_scan_code = 0x96;  // CMD+ATTN
                        else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'P')) out_scan_code = 0x91;  // CMD+PLUS
                        else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'M')) out_scan_code = 0x93;  // CMD+MINUS
                        else if ((parse_key_buffer[0] == 'C') && (parse_key_buffer[1] == 'S')) out_scan_code = 0x95;  // CMD+STAR (multiply)
                        
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '1')) { vTaskDelay(100); out_scan_code = -1; }  // DELAY 1
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '2')) { vTaskDelay(200); out_scan_code = -1; }  // DELAY 2
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '3')) { vTaskDelay(300); out_scan_code = -1; }  // DELAY 3
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '4')) { vTaskDelay(400); out_scan_code = -1; }  // DELAY 4
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '5')) { vTaskDelay(500); out_scan_code = -1; }  // DELAY 5
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '6')) { vTaskDelay(600); out_scan_code = -1; }  // DELAY 6
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '7')) { vTaskDelay(700); out_scan_code = -1; }  // DELAY 7
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '8')) { vTaskDelay(800); out_scan_code = -1; }  // DELAY 8
                        else if ((parse_key_buffer[0] == 'D') && (parse_key_buffer[1] == '9')) { vTaskDelay(900); out_scan_code = -1; }  // DELAY 9

                        else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == '0')) { interpret_crlf_as_execute = FALSE; }  // turn OFF CRLF interpretation
                        else if ((parse_key_buffer[0] == 'E') && (parse_key_buffer[1] == '1')) { interpret_crlf_as_execute = TRUE; }  // turn ON CRLF interpretation (default)

                        else                                                                   out_scan_code = -1;

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
                    else
                    {
                        // enter parse-key mode
                        parse_key_mode = TRUE;
                        parse_key_buffer_index = 0;
                        
                        out_scan_code = -1;
                        out_parity = -1;
                    }
                }
                else
                {
                    out_scan_code = -1;
                    out_parity = -1;
                }
            }

            if (out_scan_code >= 0)
            {
                //char buffer[20];
                //itoa(out_scan_code,buffer, 2);
                //printf("OUT: %02X %s %d\n",  out_scan_code, buffer, out_scan_code);  // hex binary-string and decimal

                if ((out_scan_code & 0x80) == 0x00) gpio_set_direction(PIN_KBD_0, GPIO_MODE_OUTPUT);  
                if ((out_scan_code & 0x40) == 0x00) gpio_set_direction(PIN_KBD_1, GPIO_MODE_OUTPUT);  
                if ((out_scan_code & 0x20) == 0x00) gpio_set_direction(PIN_KBD_2, GPIO_MODE_OUTPUT);
                if ((out_scan_code & 0x10) == 0x00) gpio_set_direction(PIN_KBD_3, GPIO_MODE_OUTPUT);      
                if ((out_scan_code & 0x08) == 0x00) gpio_set_direction(PIN_KBD_4, GPIO_MODE_OUTPUT);
                if ((out_scan_code & 0x04) == 0x00) gpio_set_direction(PIN_KBD_5, GPIO_MODE_OUTPUT);
                if ((out_scan_code & 0x02) == 0x00) gpio_set_direction(PIN_KBD_6, GPIO_MODE_OUTPUT);
                if ((out_scan_code & 0x01) == 0x00) gpio_set_direction(PIN_KBD_7, GPIO_MODE_OUTPUT);      
                if (out_parity == FALSE)            gpio_set_direction(PIN_KBD_P, GPIO_MODE_OUTPUT); 

                gpio_set_direction(PIN_KBD_STROBE, GPIO_MODE_OUTPUT);  // trigger ON  the STROBE for the scancode being pressed
                vTaskDelay(10);
                gpio_set_direction(PIN_KBD_STROBE, GPIO_MODE_INPUT);   // trigger OFF the STROBE

                // revert back whatever was "pulled down"
                if ((out_scan_code & 0x80) == 0x00) gpio_set_direction(PIN_KBD_0, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x40) == 0x00) gpio_set_direction(PIN_KBD_1, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x20) == 0x00) gpio_set_direction(PIN_KBD_2, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x10) == 0x00) gpio_set_direction(PIN_KBD_3, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x08) == 0x00) gpio_set_direction(PIN_KBD_4, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x04) == 0x00) gpio_set_direction(PIN_KBD_5, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x02) == 0x00) gpio_set_direction(PIN_KBD_6, GPIO_MODE_INPUT);
                if ((out_scan_code & 0x01) == 0x00) gpio_set_direction(PIN_KBD_7, GPIO_MODE_INPUT);
                if (out_parity == FALSE)            gpio_set_direction(PIN_KBD_P, GPIO_MODE_INPUT);
                
            }
            else
            {
                if (parse_key_mode == FALSE)
                {
                    // Unable to translate the key that was input
                    // could do a printf to the terminal, some kind of user feedback (flash the blue LED?)
                }
                else
                {
                    // we are waiting to PARSE a key...
                }
            }
        }

    }
}
