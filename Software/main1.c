#include MAX_MUOUSE_MULTIPLIER 3

//����� ��� ������ � ����� PS/2
//�� ������ ��������� AVR313: Interfacing the PC AT Keyboard  
//� ���������� �� http://www.computer-engineering.org

#include <mega8.h>
#include <delay.h>

//����� ������������ ������ �����-������ ��� PS/2
#define BUFF_SIZE 16

//������� ��� ����. ���� PS/2
#define P_CLK  PORTD.2
#define P_DATA PORTD.4
#define P_CLK_DIR  DDRD.2
#define P_DATA_DIR DDRD.4
#define CLK_READ PIND.2
#define DATA_READ PIND.4

//��������� ��������� �����
#define PS2error 0
#define PS2read  1
#define PS2write 2

unsigned char ps2state; //��������� ����� (�����/����/�����)
unsigned char edge; //����� ���������� �������� ��� ����������� ���������� (�����/����)
unsigned char bitcount; //������� ����� �����������
static unsigned char data;//����� �� ����
unsigned char ms_buffer[BUFF_SIZE];
unsigned char *ms_input, *ms_output; //��������� ��� ������
unsigned char buffcnt; //������� ������ ������

//��������� ���� � ����� ����
void PS2put(unsigned char c)
{
    if (buffcnt >= BUFF_SIZE) return;
    buffcnt++;
    *ms_input++ = c; // ������� � ���� ����
    if(ms_input == ms_buffer + BUFF_SIZE) ms_input = ms_buffer;
}

// �������� ���� �� ������ ����
unsigned char PS2get(void)
{
    unsigned char byte;
    if(buffcnt == 0) return 0;
    buffcnt--;
    byte = *ms_output++;
    if (ms_output >= ms_buffer + BUFF_SIZE)
        ms_output = ms_buffer;
    return byte;
}

//��������, ����� ���� ������?
unsigned char PS2ready(void)
{
    return buffcnt;
}

//��������� ��� ��������
unsigned char Parity(unsigned char p)
{
    p = p ^ (p >> 4 | p << 4);
    p = p ^ (p >> 2);
    p = p ^ (p >> 1);
    return (p ^ 1) & 1;
}

//�������� ��� � ���������� ���������� CLK PS/2
void ProcessPS2(void)
{
    //��� ������ �������� ������ ��� ��������
    static unsigned char par;

    if (ps2state == PS2read) //���������� ������
    {

        if (!edge) //��������� �� ����� �������� CLK
        {

            if (bitcount == 11) //��������� �����-���
            {
                if (DATA_READ) ps2state = PS2error; //��� �����-����? ������ ���-�� ����� �� ��� 
            }

            if((bitcount < 11) & (bitcount > 2))//������������ �������� ���� � �����
            {
                data = (data >> 1);
                if (DATA_READ) data = data | 0x80;
            }

            if (bitcount == 2) //��������� ��� ��������
            {
                if (Parity(data) != DATA_READ)
                ps2state = PS2error; //�������� �� ���������? ������ ���-�� ����� �� ��� 
            }

            if (bitcount == 1) //��������� ����-���
            {
                if (DATA_READ == 0) ps2state = PS2error; //��� ����-����? ������ ���-�� ����� �� ��� 
            }

            MCUCR = (MCUCR & 0xFC) | 3;//��������� ������� ���������� � ����� �� ������
            edge = 1;//� ����� ������ � ��������, ����������� ������������
        } else { // ��������� �� ������ �������� CLK
            MCUCR = (MCUCR & 0xFC) | 2;//��������� ������� ���������� � ����� �� �����
            edge = 0;//� ����� ������ � ��������, ����������� ������������
            if(--bitcount == 0)//��� ���� ������� �������?
            {
                PS2put(data);
                bitcount = 11; //������� ������� ����� ������
            }
        }
    }
    if (ps2state == PS2write) //���������� ������
    {
        if (!edge) //��������� �� ����� �������� CLK
        {
            if (bitcount == 11) //� ����� ������ ����� ��������� ��� �������� �� �������
                par = Parity(data);
            if((bitcount <= 11) & (bitcount > 3))//������������ �������� ���� � ���� DATA
            {
                P_DATA_DIR = (data & 1)^1;
                data = (data >> 1);
            }
            if (bitcount == 3) //������������ ��� ��������
                P_DATA_DIR = par ^ 1;
            if (bitcount == 2) //��������� ����-���
                P_DATA_DIR = 0;
            if (bitcount == 1) //���������� �������� ������ ACK
                if (DATA_READ) ps2state = PS2error; //��� ACK? ������ ���-�� ����� �� ��� 

            MCUCR = (MCUCR & 0xFC) | 3;//��������� ������� ���������� � ����� �� ������
            edge = 1;//� ����� ������ � ��������, ����������� ������������
        } else {
            MCUCR = (MCUCR & 0xFC) | 2;//��������� ������� ���������� � ����� �� �����
            edge = 0;//� ����� ������ � ��������, ����������� ������������

            if(--bitcount == 0)//��� ���� ������� ��������?
            {
                ps2state = PS2read; //��������� � ����� ������
                bitcount = 11; //� ������� ������� ����� ������
            }
        }
    }
}

// ������������� PS/2
void InitPS2(void)
{
    // ������ ���� PS/2 �������. ��� ���������� ������� ������ �������.
    P_CLK_DIR = 0;
    P_DATA_DIR = 0;

    ms_input = ms_buffer; // ������������� ��������� �� �����
    ms_output = ms_buffer;
    buffcnt = 0;

    GIFR = 0x40;
    GICR |= 0x40;
    MCUCR = (MCUCR & 0xFC) | 2;//������������� ������� ���������� � ����� �� �����

    ps2state = PS2read; //������������� ������ - ������
    edge = 0; //������������ ��������, ����������� ������������ (������ �� ����� CLK)
    bitcount = 11; //������� ������� �����
}

//������ ����� � PS/2
void WritePS2(unsigned char a)
{
    //�������� �������� ���������� �� CLK
    GIFR = 0x40;
    GICR &= 0xBF;

    //����� CLK �� �����
    P_CLK = 0;
    P_CLK_DIR = 1;

    //������ ����� ����
    while (PS2ready())
    PS2get();

    //����� ������ ���� ����
    data = a;

    //��� � ������� 100 ���
    delay_us(100);

    //����� DATA �� �����
    P_DATA = 0;
    P_DATA_DIR = 1;

    //��������� CLK
    P_CLK_DIR = 0;

    GIFR = 0x40;
    GICR |= 0x40;
    MCUCR = (MCUCR & 0xFC) | 2;//������������� ������� ���������� � ����� �� �����

    ps2state = PS2write; //������������� ������ - ������
    edge = 0; //������������ ��������, ����������� ������������ (������ �� ����� CLK)
    bitcount = 11; //������� ������� �����
}

//������� ��������� �������� ������������� �������� ������ � ����
//(���������� �������� � �������� ������ "0xFA")
void FAskip(void)
{
    while (PS2ready() < 1) ; //��� �����
    if (PS2get() != 0xFA) ps2state = PS2error; //��������� �����
}

unsigned char ps2_recv()
{
    while (PS2ready() < 1) ; //��� �����
    return PS2get();
}

void ps2_send(unsigned char x)
{
    WritePS2(x);
    FAskip();
}

// ������������� ����. ���������� 0/1, ���� ���� ��������� 3/4 �����.

unsigned char mouse_init()
{
    unsigned char sample_rate; //��������� �������� �������� ������ ������
    unsigned char has_wheel; 

    InitPS2(); // ������� ������������� ����������

    // ����� �����
    ps2_send(0xFF);
    if(ps2_recv() != 0xAA) ps2state = PS2error;
    if(ps2_recv() != 0x00) ps2state = PS2error;

    // �������� �������� ������ ����������
    ps2_send(0xFF);
    ps2_recv(); //������ ������ �� ����������
    ps2_recv(); //���������� �� ����������
    sample_rate = ps2_recv(); //���������� �������� �������� ������ ������ 

    // �������� ����������� ����� ����, � ������� �������� ������
    ps2_send(0xF3);
    ps2_send(0xC8);
    ps2_send(0xF3);
    ps2_send(0x64);
    ps2_send(0xF3);
    ps2_send(0x50);

    // ��������� �� ����������� �����?
    ps2_send(0xF2);
    has_wheel = ps2_recv() ? 1 : 0;

    // ��������� �������� ������ ������
    ps2_send(0xF3); 
    ps2_send(sample_rate);
    
    // ������������� ����� ������� ���������� 8 �������� �� 1 �� 
    ps2_send(0xE8);
    ps2_send(0x03); 
              
    // �������� ��������� ����� ��������
    ps2_send(0xF4);
              
    return has_wheel;
}

/*****************************************************
This program was produced by the
CodeWizardAVR V2.04.4a Advanced
Automatic Program Generator
� Copyright 1998-2009 Pavel Haiduc, HP InfoTech s.r.l.
http://www.hpinfotech.com

Project : PS/2 to COM converter
Version : 2.2
Date    : 01.03.2015
Author  : E.J.SanYo
Company : 
Comments: 
������� ���������

29.04.2015 2.2.1:
           ��������� �������. ������ ������ ���������� ��� ����������
           ���� ���� - ����������.

01.03.2015 2.2:
           ������ "����� �����" �������� �������� ������� PS/2, 
           �������������, ��� ������ � ��������� ��� � �������
           ��������� �� �����. ��������� ����� ����� ��������� �������
           ������� ������� PS/2 � "������" ��� ����� ���������� ���������.
           (������� �������� � ������������ ������� � ��������� �����).
           ��� ���� ��-�� �������� ������������� ���������������� 
           ��������� ������� "������ ���������������" �����������.

27.02.2015 2.1:
           ��������������� �������� ������� ������� PS/2 ��� ������
           ������� ������. ��������� ����������� ���������������� �
           ������ ����������� ������� (������� ����������� �� �����������).
           ��������� ������� ������ �� ������������ ������ COM �����.
           ����� �������� watchdog ��� ������ ��� ��������� ������ �� PS/2. 

26.02.2015 2.1 alpha:
           ��������� ����������� �������� ������ � PS/2. ������� 
           ��� �� ������ �� �����������, ����������� ���������� 
           �� ��������� ����� ���������. ���������� ���������� 
           ��������� ����� ������ ����. �������� ������ �����������
           ����������������. ��������� ���������� �������.

22.02.2015 2.00 beta:
           �������� ����������������� ����� �������� ���� EM84520
           (���� �� ����������). ������������ ��������� ���������
           ��� ��������� �������

20.02.2015 2.00 alpha2:
           ��������� ������������ ����������� ��� ����� ������ �� PS/2.

19.02.2015 2.00 alpha:
           ���� ���������� ������ �������� ��� ����� ������������.

24.11.2014 1.00:
           �������� ����� ������ � "��������������� ����������" 
           � ���������� ���������������� ���� ��� (4 ����������� �������� PS/2.
           ������ ��������� ����� ����������� � EEPROM �����������.
           SB1 - ��������� ����������������, SB2 - ���������, SB3 - ���������

22.11.2014 0.91 alpha 2:
           ��������� � ������������ ��� "�����������" ������ Windows 3.1
           ��������� ��������� ������� ������ ����

20.11.2014 0.9 alpha:
           ��������� ����������� � ������ � ������������ UART �� ������� �� CodeVision ;)
           ������� �������� ������� ������ � PS/2
           �������� watchdog �� ������, ���� ����� PS/2 ������� �������� 



Chip type               : ATmega8
Program type            : Application
AVR Core Clock frequency: 14,745600 MHz
Memory model            : Small
External RAM size       : 0
Data Stack size         : 256
*****************************************************/

#include <mega8.h>
#include <delay.h>

flash char EM84520_ID[61] = {
    0x4D, 0x5A, 0x40, 0x00, 0x00, 0x00,
    0x08, 0x01, 0x24, 0x25, 0x2D, 0x23,
    0x10, 0x10, 0x10, 0x11, 0x3C, 0x3C,
    0x2D, 0x2F, 0x35, 0x33, 0x25, 0x3C,
    0x30, 0x2E, 0x30, 0x10, 0x26, 0x10,
    0x21, 0x3C, 0x25, 0x2D, 0x23, 0x00,
    0x33, 0x23, 0x32, 0x2F, 0x2C, 0x2C,
    0x29, 0x2E, 0x27, 0x00, 0x33, 0x25,
    0x32, 0x29, 0x21, 0x2C, 0x00, 0x2D,
    0x2F, 0x35, 0x33, 0x25, 0x21, 0x15,
    0x09
};

//������������� ������ �������� ������������ ��� �������� ����
eeprom unsigned char mouse_multiplier_eeprom;

//��� �� ��������, ������ � ��� ��� ������� ��������
unsigned char ps2_divider_current;

//���������� ��� ������ ���������
//������ 1 - ��� EM84520, 0 - ��� logitech
char output_protocol;

//���������� ������ ������� ��������� ������� ������ ����
//��� ������������� M$ Mouse ���������.
char msmb_old = 0;

//��� ��� ���������� �������� ������ �� PS/2 � COM
bit rs232_enabled = 1;

//������� ��� "������������" ������
char butt_counter = 0;

//�������� ������� ������
//1 - SB1, 2 - SB2, 3 - SB3, 0 - ������ �� ������
char butt_value = 0;

void putchar(char c);


// External Interrupt 0 service routine
interrupt [EXT_INT0] void ext_int0_isr(void)
{
//��������� ��������� ���������� �������� CLK
ProcessPS2();

}

//-------------------------------------------------------------------------------------
// External Interrupt 1 service routine

interrupt [EXT_INT1] void ext_int1_isr(void)
{
    char i;             
    rs232_enabled = !PIND.3; 
    if(rs232_enabled)
    {
        delay_ms(14);
        if(output_protocol)
        {
            // ���������� ����������� ��������� EM84520
            for (i=0; i<sizeof(EM84520_ID); i++)
                putchar(EM84520_ID[i]);
        } else {
            // ���������� ����������� ��������� MS Mouse
            putchar('M');
            delay_ms(63);
            putchar('3');
        }
    }
}

#ifndef RXB8
#define RXB8 1
#endif

#ifndef TXB8
#define TXB8 0
#endif

#ifndef UPE
#define UPE 2
#endif

#ifndef DOR
#define DOR 3
#endif

#ifndef FE
#define FE 4
#endif

#ifndef UDRE
#define UDRE 5
#endif

#ifndef RXC
#define RXC 7
#endif

#define FRAMING_ERROR (1<<FE)
#define PARITY_ERROR (1<<UPE)
#define DATA_OVERRUN (1<<DOR)
#define DATA_REGISTER_EMPTY (1<<UDRE)
#define RX_COMPLETE (1<<RXC)

// USART Transmitter buffer
#define TX_BUFFER_SIZE 16
char tx_buffer[TX_BUFFER_SIZE];

#if TX_BUFFER_SIZE<256
unsigned char tx_wr_index,tx_rd_index,tx_counter;
#else
unsigned int tx_wr_index,tx_rd_index,tx_counter;
#endif

// USART Transmitter interrupt service routine
interrupt [USART_TXC] void usart_tx_isr(void)
{
    if (tx_counter)
    {
        --tx_counter;
        UDR = tx_buffer[tx_rd_index];
        if(++tx_rd_index == TX_BUFFER_SIZE) tx_rd_index=0;
    };
}

#ifndef _DEBUG_TERMINAL_IO_
// Write a character to the USART Transmitter buffer
#define _ALTERNATE_PUTCHAR_
#pragma used+
void putchar(char c)
{
while (tx_counter == TX_BUFFER_SIZE);
#asm("cli")
if (tx_counter || ((UCSRA & DATA_REGISTER_EMPTY)==0))
   {
   tx_buffer[tx_wr_index]=c;
   if (++tx_wr_index == TX_BUFFER_SIZE) tx_wr_index=0;
   ++tx_counter;
   }
else
   UDR=c;
#asm("sei")
}
#pragma used-
#endif

//-------------------------------------------------------------------------------------
// Timer 0 overflow interrupt service routine

interrupt [TIM0_OVF] void timer0_ovf_isr(void)
{
    if(button_disabled == 0) return;
    if((PINB & 7) != 7) button_disabled = BUTTON_DISABLED;
    button_disabled--;
}

//-------------------------------------------------------------------------------------
// Timer1 output compare A interrupt service routine

interrupt [TIM1_COMPA] void timer1_compa_isr(void)
{
}

//-------------------------------------------------------------------------------------
// ������ ������� ����������

interrupt [TIM2_OVF] void timer2_ovf_isr(void)
{
    TCCR2 = 0;     // ��������� �������    
    PORTB.3 = 1;   // ������� ����������
}

//-------------------------------------------------------------------------------------
// �������� ������ �� ��������� MS Mouse + Logitech

void SendCoordMS(signed char x, signed char y, char LB, char RB, char MB)
{
    //����������� ����� ��������� 
    putchar((1 << 6) | (LB << 5) | (RB << 4) | ((y & 0xC0) >> 4) | ((x & 0xC0) >> 6));
    putchar(x & 0x3F);
    putchar(y & 0x3F);
    
    // ���������� ��� 3-� ������, ��������� ��� Logitech, ��� Microsoft Plus
    // 3 ������ ������ ��� ������ ��� ��������? 
    if (MB || msmb_old)
    {
        putchar(MB << 5);
        msmb_old = MB;
    } 
}

//-------------------------------------------------------------------------------------

//������� ����� ���. ����� �� ��������� ���� EM84520
void SendCoordEM84520(signed char x, signed char y, signed char z, char LB, char RB, char MB)
{
    // ����������� ����� ���������, ��� � M$ mouse 
    putchar((1 << 6) | (LB << 5) | (RB << 4) | ((y & 0xC0) >> 4) | ((x & 0xC0) >> 6));
    putchar(x & 0x3F);
    putchar(y & 0x3F);

    // ���������� ���� EM84520
    putchar((MB << 4) | (z & 0x0F)); 
}

//-------------------------------------------------------------------------------------
//�������� ������ � �� ������ �� ��� ���, ���� �� �������� ��������

void SetLED(void)
{
    PORTB.3 = 0;
    TCNT2 = 0x00;
    TCCR2 = 0x37;
}

//-------------------------------------------------------------------------------------
// �������� ����� ������� ��������� ������

unsigned char get_onboard_button()
{
    unsigned char n;      
    // ��������� ������� ���������
    if(button_disabled) return 0;
    // ����� ������� ������
    if(!PINB.0) n=1; else 
    if(!PINB.1) n=2; else
    if(!PINB.2) n=3; else return 0;
    // ���� ���� ������ ������, ������ �����������
    SetLED();
    // � ��������� ����� �� ��������� �� �������
    button_disabled = BUTTON_DISABLED;
    return n;
}

//-------------------------------------------------------------------------------------

void main(void)
{
    char x, y, z, mouse_b;
    short mouse_x, mouse_y, mouse_z;
    char mouse_b_out;
              
    // ������������� ������
    DDRB = 0x08; PORTB = 0x38;
    DDRC = 0x00; PORTC = 0x7F;
    DDRD = 0x02; PORTD = 0x62;

    // Timer/Counter 0 initialization
    // Clock source: System Clock
    // Clock value: 57,600 kHz
    TCCR0 = 0x04;
    TCNT0 = 0x00;

    // Timer/Counter 1 initialization
    // Clock source: System Clock
    // Clock value: 14,400 kHz
    // Mode: CTC top=OCR1A
    // OC1A output: Discon.
    // OC1B output: Discon.
    // Noise Canceler: Off
    // Input Capture on Falling Edge
    // Timer1 Overflow Interrupt: Off
    // Input Capture Interrupt: Off
    // Compare A Match Interrupt: On
    // Compare B Match Interrupt: Off
    TCCR1A=0x00;
    TCCR1B=0x0D;
    TCNT1H=0x00;
    TCNT1L=0x00;
    ICR1H=0x00;
    ICR1L=0x00;
    OCR1AH=0x02;
    OCR1AL=0xD0;
    OCR1BH=0x00;
    OCR1BL=0x00;

    // Timer/Counter 2 initialization
    // Clock source: System Clock
    // Clock value: 14,400 kHz
    // Mode: Normal top=FFh
    // OC2 output: Disconnected
    ASSR=0x00;
    TCCR2=0x07;
    TCNT2=0x00;
    OCR2=0x00;

    // External Interrupt(s) initialization
    // INT0: Off
    // INT1: On
    // INT1 Mode: Any change
    GICR|=0x80;
    MCUCR=0x04;
    GIFR=0x80;

    // Timer(s)/Counter(s) Interrupt(s) initialization
    TIMSK=0x51;

    //�� ���� ����� �������� �������� ������������ COM-������� ����
    output_protocol = PIND.7 ^ 1;

    // ��������� COM-����� �� 1200, 7 Data, No Parity
    UCSRA=0x00;
    UCSRB=0x48;
    UCSRC=output_protocol ? 0x8C : 0x84; // 2 Stop / 1 Stop 
    UBRRH=0x02;
    UBRRL=0xFF;

    // Analog Comparator initialization
    // Analog Comparator: Off
    // Analog Comparator Input Capture by Timer/Counter 1: Off
    ACSR=0x80;
    SFIOR=0x00;

    // Watchdog Timer initialization
    // Watchdog Timer Prescaler: OSC/1024k
    #pragma optsize-
    WDTCR=0x1E;
    WDTCR=0x0E;
    #ifdef _OPTIMIZE_SIZE_
    #pragma optsize+
    #endif

    // ��������� �������� �������� �� EEPROM
    mouse_mul = ps2_divider;
    
    // ������������� ��� ��������, ���� ���������
    if(mouse_mul > 2) mouse_mul = 1; 

    // Global enable interrupts
    #asm("sei")

    //��������� ��������� ���� PS/2
    input_protocol = mouse_init();

    //�������� ����� �������������
    SetLED();
    
    // ����� ��������� ����
    mouse_x = 0, mouse_y = 0, mouse_z = 0;

    for(;;)
    {
        // ��������� ����������� �� PS/2 ������
        if(PS2ready() > (2 + input_protocol))
        {
            mouse_b = PS2get();
            mouse_x += PS2get() * mouse_mul;
            mouse_y -= PS2get() * mouse_mul;
            if(input_protocol) mouse_z += PS2get();

            // �������� ��������
            SetLED();
        }
    
        // ���������� ����� ����������
        // - ���� ����� �������� ���� (c������� PS/2 ���� ������, ��� COM)
        // - ���� ��������� �������� ��������
        // - ���� ��������� ���� ����������
        // - ���� ��������� ������ ����������
        if(tx_counter == 0 && rs232_enabled && ((mouse_b != mouse_b_old) || (mouse_x != 0) || (mouse_y != 0) || (mouse_z != 0)))
        {
            // ������������� ������������ � ������ ���������
            x = mouse_x<-128 ? -128 : (mouse_x>127 ? 127 : mouse_x); mouse_x -= x;
            y = mouse_y<-128 ? -128 : (mouse_y>127 ? 127 : mouse_y); mouse_y -= y;
            z = mouse_z<-8 ? -8 : (mouse_z>7 ? 7 : mouse_z); mouse_z -= z;
            mouse_b_old = mouse_b;

            // �������� � COM-����
            if (output_protocol)
                SendCoordEM84520(x, y, z, mouse_b & 1, (mouse_b >> 1) & 1, (mouse_b >> 2) & 1);
            else
                SendCoordMS(x, y, mouse_b & 1, (mouse_b >> 1) & 1, (mouse_b >> 2) & 1);                
        }
              
        // ��������� ������ �� �����
        switch(get_onboard_button())
        {
            // ������ ������ SB1 - ��������� ���������������� ����
            case 1:
                if(mouse_mul > 0) mouse_mul--;
                break;
            // ������ ������ SB2 - ����������� ���������������� ����
            case 2: 
                if(mouse_mul < MAX_MUOUSE_MULTIPLIER-1) mouse_mul++;
                break;
            // ������ ������ SB3 - ��������� ���������������� � EEPROM
            case 4:
                mouse_multiplier_eeprom = mouse_mul;
                break;
        }
         
        // ���������������, ���� ����� PS/2     
        if(ps2state)
        {
            #asm("wdr");
        }
    };
}
