// (�) 30-08-2015 Morozov Aleksey @ vinxru
// aleksey.f.morozov@gmail.com
// AS IS

#include <mega8.h>
#include <delay.h>
#include <stdint.h>

//===========================================================================
// PS/2 ����
//===========================================================================

#define PS2_BUF_SIZE 16  // ������ �������� ������ PS/2 �����

#define PS2_CLK_PORT  PORTD.2 // ����� � ������� �������� �������� ���. PS/2
#define PS2_CLK_DDR   DDRD.2
#define PS2_CLK_PIN   PIND.2
#define PS2_DATA_PORT PORTD.4 // ����� � ������� �������� ������ ������ PS/2 
#define PS2_DATA_DDR  DDRD.4
#define PS2_DATA_PIN  PIND.4

enum ps_state_t { ps2_state_error, ps2_state_read, ps2_state_write };

uint8_t ps2_state;                // ��������� ����� (ps_state_t)
uint8_t ps2_bitcount;             // ������� ����� �����������
uint8_t ps2_data;                 // ����� �� ����
uint8_t ps2_parity;
uint8_t ps2_rx_buf[PS2_BUF_SIZE]; // �������� ����� PS/2 �����
uint8_t ps2_rx_buf_w;
uint8_t ps2_rx_buf_r;
uint8_t ps2_rx_buf_count;

//---------------------------------------------------------------------------
// ��������� �������� ���� � ����� ����� PS/2 �����. ���������� ������ ��
// ����������� ����������.

void ps2_rx_push(uint8_t c) {
    // ���� ����� ���������� � ������� ����, �� ��������� �� ������ ��������� 
    // ������������ ��� ���������� ������, ������� ������������� ����������.
    if(ps2_rx_buf_count >= sizeof(ps2_rx_buf)) {
	    ps2_state = ps2_state_error;
	    return;
    }
    // ���������� � �����
    ps2_rx_buf[ps2_rx_buf_w] = c;
    ps2_rx_buf_count++;
    if(++ps2_rx_buf_w == sizeof(ps2_rx_buf)) ps2_rx_buf_w = 0;
}

//---------------------------------------------------------------------------
// �������� ���� �� �������� ������ PS/2 �����

uint8_t ps2_aread(void) {
    uint8_t d;
    // ��������� ����������, ��� ��� ���������� ���������� 
    // �� �� ������������ ��� ����������.
    #asm("cli")
    // ���� ����� ����, ���������� ����
    if(ps2_rx_buf_count == 0) {
        d = 0;
    } else {
        // ������ ���� �� ������
        d = ps2_rx_buf[ps2_rx_buf_r];
        ps2_rx_buf_count--;
        if(++ps2_rx_buf_r == sizeof(ps2_rx_buf)) ps2_rx_buf_r = 0;
    }
    // �������� ����������
    #asm("sei")
    return d;
}

//---------------------------------------------------------------------------
//��������, ����� ���� ������?

uint8_t ps2_ready(void) {
    return ps2_rx_buf_count;
}

//---------------------------------------------------------------------------
// ���������� ���� ��������

uint8_t parity(uint8_t p) {
    p = p ^ (p >> 4 | p << 4);
    p = p ^ (p >> 2);
    p = p ^ (p >> 1);
    return (p ^ 1) & 1;
}

//---------------------------------------------------------------------------
// ��������� ��������� ������� PS/2

#ifndef NO_INT
interrupt [EXT_INT0]
#endif
void ext_int0_isr(void) {
    if(ps2_state == ps2_state_error) return;

    if(ps2_state == ps2_state_write) {
        switch(ps2_bitcount) {
            default: // ������
                PS2_DATA_DDR = (ps2_data & 1) ^ 1;
                ps2_data >>= 1;
                break;
            case 3: // ��� ��������
                PS2_DATA_DDR = ps2_parity ^ 1;
                break;
            case 2: // ���� ���
                PS2_DATA_DDR = 0;
                break;
            case 1: // ������������� �����
                if(PS2_DATA_PIN) ps2_state = ps2_state_error;
                            else ps2_state = ps2_state_read; 
                ps2_bitcount = 12;
        } 
    } else {
        switch(ps2_bitcount) {
            case 11: // ����� ���
                if(PS2_DATA_PIN) ps2_state = ps2_state_error;
                break;
            default: // ������
                ps2_data >>= 1;
                if(PS2_DATA_PIN) ps2_data |= 0x80;
                break;
            case 2: // ��� �������� 
                if(parity(ps2_data) != PS2_DATA_PIN) ps2_state = ps2_state_error;
                break;
            case 1: // ���� ��� 
                if(PS2_DATA_PIN) ps2_rx_push(ps2_data);
                            else ps2_state = ps2_state_error;  
                ps2_bitcount = 12;
        }
    }
    ps2_bitcount--;
}

//---------------------------------------------------------------------------
// ������������� PS/2

void ps2_init(void) {
    // ����������� PS/2 ���� �� ����
    PS2_CLK_DDR = 0;
    PS2_DATA_DDR = 0;
    
    // ������� ������� �����
    ps2_rx_buf_w = 0;
    ps2_rx_buf_r = 0;
    ps2_rx_buf_count = 0;
    
    // ������������ ���������� ����������� ����������
    ps2_state = ps2_state_read;
    ps2_bitcount = 11;

    // ���������� �� ����� ��������� �������
    GIFR = 0x40;
    GICR |= 0x40;
    MCUCR = (MCUCR & 0xFC) | 2;
}

//---------------------------------------------------------------------------
// �������� ����� � PS/2 ���� ��� �������������

void ps2_write(uint8_t a) {
    // ��������� ���������� �� ��������� ��������� ������� PS/2
    GIFR = 0x40;
    GICR &= 0xBF;

    // �������� �������� ������ PS/2 �� �����
    PS2_CLK_PORT = 0;
    PS2_CLK_DDR = 1;

    // ��� � ������� 100 ���
    delay_us(100);

    // �������� ����� ������ PS/2 �� �����
    PS2_DATA_PORT = 0;
    PS2_DATA_DDR = 1;

    // ����������� �������� ������
    PS2_CLK_DDR = 0;

    // ������� ������� �����
    ps2_rx_buf_count = 0;
    ps2_rx_buf_w = 0;
    ps2_rx_buf_r = 0;

    // ����������� ���������� ����������� ����������
    ps2_state = ps2_state_write;
    ps2_bitcount = 11;
    ps2_data = a;             
    ps2_parity = parity(a);

    // �������� ���������� �� ����� ��������� ������� PS/2
    GIFR = 0x40;
    GICR |= 0x40;
    MCUCR = (MCUCR & 0xFC) | 2;    
}

//---------------------------------------------------------------------------
// ��������� ����� �� PS/2 ����� � ���������

uint8_t ps2_recv(void)
{
    while(ps2_rx_buf_count == 0);
    return ps2_aread();
}

//---------------------------------------------------------------------------
// �������� ����� � PS/2 ���� � ��������������

void ps2_send(uint8_t c) {
    ps2_write(c);
    if(ps2_recv() != 0xFA) ps2_state = ps2_state_error;
}

//===========================================================================
// RS232 ����
//===========================================================================

#define RS232_TX_BUFFER_SIZE 16

uint8_t rs232_tx_buf[RS232_TX_BUFFER_SIZE];
uint8_t rs232_tx_buf_w;
uint8_t rs232_tx_buf_r;
uint8_t rs232_tx_buf_count;
uint8_t rs232_reset;
uint8_t rs232_enabled;

#pragma used+
void rs232_send(uint8_t c) {
    // ���������, ���� ����� ����������
    while(rs232_tx_buf_count == sizeof(rs232_tx_buf));

    // ��������� ����������, ��� ��� ���������� ���������� 
    // �� �� ������������ ��� ����������.
    #ifndef NO_INT
    #asm("cli")
    #endif

    // ���� �������� ��� ���� ��� � ������ �������� ��� �� ����,
    // �� ��������� � ����� ��������
    if (rs232_tx_buf_count || ((UCSRA & (1 << UDRE)) == 0)) {
        rs232_tx_buf[rs232_tx_buf_w] = c;
        if(++rs232_tx_buf_w == sizeof(rs232_tx_buf)) rs232_tx_buf_w = 0;
        rs232_tx_buf_count++;
    } else {
        // ����� ������� �������� � ����
        UDR = c;
    }

    // �������� ����������
    #ifndef NO_INT
    #asm("sei")
    #endif
}
#pragma used-

//---------------------------------------------------------------------------
// COM-���� �������� ����

#ifndef NO_INT
interrupt [USART_TXC] 
#endif
void rs232_interrupt(void) {
    // ���� ����� ����, �� ������ �� ������
    if(!rs232_tx_buf_count) return;

    // ����� ���������� ���� �� ������
    --rs232_tx_buf_count;
    UDR = rs232_tx_buf[rs232_tx_buf_r];
    if(++rs232_tx_buf_r == sizeof(rs232_tx_buf)) rs232_tx_buf_r = 0;
}

//---------------------------------------------------------------------------
// ������������ ��������� ����� DTR ��� RTS

#ifndef NO_INT
interrupt [EXT_INT1] 
#endif
void ext_int1_isr(void) {
    // ��������� ��������� � ����������
    rs232_enabled = (PIND & 8) ? 0 : 1;
    // ��������� ������� ������
    rs232_reset = 1;          
}

//===========================================================================
// ��������� ���������
//===========================================================================

#ifndef NO_INT
#define PORT_LED PORTB.3
#endif

// �������� ���������
void flash_led(void) {
    PORT_LED = 0;
    TCNT2 = 0x00;
    TCCR2 = 0x37;
}

//---------------------------------------------------------------------------
// ���������� ���������� ����� ��������� �����

#ifndef NO_INT
interrupt [TIM2_OVF]
#endif
void timer2_ovf_isr(void) {
    TCCR2 = 0;
    PORT_LED = 1;
}

//===========================================================================
// ������������� PS/2 ����
//===========================================================================

uint8_t  ps2m_protocol;   // ������������ ��������: 0=��� ������, 1=� �������
uint8_t  ps2m_multiplier; // ��������������� ���������
uint8_t  ps2m_b;          // ������� ������
int16_t  ps2m_x, ps2m_y, ps2m_z; // ����������

//---------------------------------------------------------------------------
// ������������� PS/2 ����

void ps2m_init() {
    // �������� ������� "�����".
    ps2_send(0xFF);
    if(ps2_recv() != 0xAA) { ps2_state = ps2_state_error; return; }
    if(ps2_recv() != 0x00) { ps2_state = ps2_state_error; return; }

    // �������� ������ � ������� ������������ 80 ������� � �������.    
    ps2_send(0xF3);
    ps2_send(0xC8);
    ps2_send(0xF3);
    ps2_send(0x64);
    ps2_send(0xF3);
    ps2_send(0x50);

    // ������, ���������� �� �������� ������
    ps2_send(0xF2);
    ps2m_protocol = ps2_recv() ? 1 : 0;

    // ���������� 8 ����� �� ��
    ps2_send(0xE8);
    ps2_send(0x03);

    // 20 �������� � ��� (���� ���������� ��� �������)
    ps2_send(0xF3);
    ps2_send(20);
               
    // �������� ��������� �����.
    ps2_send(0xF4);
}

//---------------------------------------------------------------------------
// ��������� ����������� � PS/2 ����� ������

void ps2m_process() {
    while(ps2_ready() >= (3 + ps2m_protocol)) {
        ps2m_b = ps2_aread() & 7; //! ��� ������� ����!!!
        ps2m_x += (int8_t)ps2_aread();
        ps2m_y -= (int8_t)ps2_aread();
        if(ps2m_protocol) ps2m_z += (int8_t)ps2_aread();
    }
}          

//===========================================================================
// ��������� ������
//===========================================================================

#define BUTTONS_TIMEOUT 50

uint8_t buttons_disabled = 0;    // ������� ��� �������������� ��������
uint8_t pressed_button   = 0xFF; // ��������� ������� ������

// ������������� ����� ��������� ������
#ifndef NO_INT
interrupt [TIM0_OVF] 
#endif
void timer0_ovf_isr(void) {
    uint8_t b = (PINB & 7) ^ 7;
    if(!buttons_disabled) {
        if(b & 1) pressed_button = 0; else
        if(b & 2) pressed_button = 1; else
        if(b & 4) pressed_button = 2; else return;
    }
    if(b) buttons_disabled = BUTTONS_TIMEOUT;
    buttons_disabled--;
}

//===========================================================================
// ��������� � �����������
//===========================================================================

uint8_t rs232m_protocol; // ������������ ��������: 0=MSMouse, 1=EM84520

const uint8_t EM84520_ID[61] = {
    0x4D, 0x5A, 0x40, 0x00, 0x00, 0x00, 0x08, 0x01, 0x24, 0x25, 0x2D, 0x23,
    0x10, 0x10, 0x10, 0x11, 0x3C, 0x3C, 0x2D, 0x2F, 0x35, 0x33, 0x25, 0x3C,
    0x30, 0x2E, 0x30, 0x10, 0x26, 0x10, 0x21, 0x3C, 0x25, 0x2D, 0x23, 0x00,
    0x33, 0x23, 0x32, 0x2F, 0x2C, 0x2C, 0x29, 0x2E, 0x27, 0x00, 0x33, 0x25,
    0x32, 0x29, 0x21, 0x2C, 0x00, 0x2D, 0x2F, 0x35, 0x33, 0x25, 0x21, 0x15,
    0x09
};

//---------------------------------------------------------------------------

void rs232m_init() {
    // �������� ������������ ���������� �� �����
    rs232m_protocol = (PIND & 0x80) ? 0 : 1;

    // ��������� RS232: 1200 ���, 1/2 ���� ����, 7 ���, ��� ��������
    UCSRA = 0;
    UCSRB = 0x48;
    UCSRC = rs232m_protocol ? 0x8C : 0x84; 
    UBRRH = 0x02;
    UBRRL = 0xFF;
    
    // �� ��������� �������
    rs232_enabled = 1;

    // ������� �����������
    rs232_reset = 1;    
}

//---------------------------------------------------------------------------

void rs232m_send(int8_t x, int8_t y, int8_t z, uint8_t b) {
    uint8_t i, lb, rb, mb;
    static uint8_t mb1;
    
    // ��������� ������      
    if(rs232_reset) {
        rs232_reset = 0; 
        delay_ms(14);
        if (rs232m_protocol) {
            // ����������� EM84520
            for(i=0; i<sizeof(EM84520_ID); i++)
                rs232_send(EM84520_ID[i]);
        } else {
            // ����������� Logitech/Microsoft Plus
            rs232_send(0x4D);
            delay_ms(63);
            rs232_send(0x33);
        }
    }
    
    // ������� ����
    lb = b & 1;
    rb = (b >> 1) & 1;
    mb = (b >> 2) & 1;

    // ����������� ����� ��������� 
    rs232_send((1 << 6) | (lb << 5) | (rb << 4) | ((y & 0xC0) >> 4) | ((x & 0xC0) >> 6));
    rs232_send(x & 0x3F);
    rs232_send(y & 0x3F);
    
    if(rs232m_protocol) {
        // ���������� EM84520
        rs232_send((mb << 4) | (z & 0x0F));
    } else { 
        // ���������� Logitech/Microsoft Plus
        if(mb || mb1) {
            rs232_send(mb << 5);
            mb1 = mb;
        }
    }                    
}

//===========================================================================
// ���������
//===========================================================================

void init(void) {
    // ��������� ������ �����-������ � ������������� ����������
    DDRB = 0x08; PORTB = 0x3F;
    DDRC = 0x00; PORTC = 0x7F;
    DDRD = 0x02; PORTD = 0xE2;

    // Timer/Counter 0 initialization
    // Clock source: System Clock
    // Clock value: 57,600 kHz
    TCCR0 = 4;
    TCNT0 = 0;

    // ������ 2
    ASSR = 0; TCCR2 = 7; TCNT2 = 0; OCR2 = 0;

    // ��������� ���������� �� ��������� �� ����� INT1
    GICR |= 0x80; MCUCR = 0x04; GIFR = 0x80;

    // Timer(s)/Counter(s) Interrupt(s) initialization
    TIMSK = 0; 
    
    // �������� ���������� �� �������� 0 � 2
    TIMSK |= 0x41;                       

    // Analog Comparator initialization
    // Analog Comparator: Off
    // Analog Comparator Input Capture by Timer/Counter 1: Off
    ACSR = 0x80;
    SFIOR = 0;

    // ��������� Watchdog-�������
    #pragma optsize-
    WDTCR = 0x1E;
    WDTCR = 0x0E;
    #ifdef _OPTIMIZE_SIZE_
    #pragma optsize+
    #endif

    // ��������� ����������
    #ifndef NO_INT
    #asm("sei")
    #endif
}

//---------------------------------------------------------------------------

eeprom uint8_t eeprom_ps2m_multiplier;

void main(void) {
    int8_t cx, cy, cz;
    uint8_t mb1=0;
    
    // �������������� ���������
    ps2m_multiplier = eeprom_ps2m_multiplier;
    if(ps2m_multiplier > 2) ps2m_multiplier = 1; 
    
    // ������
    init();
    
    // ������ PS/2 �����
    ps2_init();
    
    // ������ ��������� PS/2 ����
    ps2m_init();
    
    // ������ RS232 ����� � ��������� COM-����
    rs232m_init();
    
    // �������� ����� �������������
    flash_led();    

    for(;;) {
        // ������ ������ �� PS/2
        ps2m_process();
        
        // ������������� �������� ���� ����� � ����
        if(rs232m_protocol==0 && (ps2m_b & 3) == 3)
        {
            if(ps2m_z < 0) { if(ps2m_multiplier > 0) ps2m_multiplier--; ps2m_z=0; } else
            if(ps2m_z > 0) { if(ps2m_multiplier < 2) ps2m_multiplier++; ps2m_z=0; }
        }
        
        // ���������� ��������� �����, ���� ����� �������� ����, ���� ��������, 
        // ���������� ������� ������ ��� ��������� ����
        if(rs232_enabled)
        if(ps2m_b != mb1 || ps2m_x != 0 || ps2m_y != 0 || ps2m_z != 0 || rs232_reset) {
            if(rs232_tx_buf_count == 0) {
                cx = ps2m_x<-128 ? -128 : (ps2m_x>127 ? 127 : ps2m_x); ps2m_x -= cx;
                cy = ps2m_y<-128 ? -128 : (ps2m_y>127 ? 127 : ps2m_y); ps2m_y -= cy;
                cz = ps2m_z<-8   ? -8   : (ps2m_z>7   ?   7 : ps2m_z); ps2m_z -= cz;
                mb1 = ps2m_b;
                rs232m_send(cx, cy, cz, ps2m_b);
            } else {
                flash_led();
            }
        }

        // ��������� ��������� ������
        if(pressed_button != 0xFF) {
            ps2m_multiplier = pressed_button;
            eeprom_ps2m_multiplier = ps2m_multiplier;
            flash_led();
            pressed_button = 0xFF;
        }
                   
        // � ������ ������ ���������������
        if(ps2_state != ps2_state_error) {
           #ifndef NO_INT
           #asm("wdr");
           #endif
        }
    }
}
