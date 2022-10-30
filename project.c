#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

// global variables
#define atoms_MAX 20
#define volume_MAX 100
#define temperature_MAX 600
#define gas_constant 8.314

// x y coordinates of the center of 3*3 squares
int x[atoms_MAX], y[atoms_MAX];
// coefficient that is displayed on HEX
short int selected_coef = 0;
// pressure in Pa
int pressure = 498;
// volume in m^3
int volume = 100;
// temperature in K
int temperature = 300; 
// speed of boxes in pixel per frame
short int speed = 3;
// velocities of boxes in pixel per frame
short int dx[atoms_MAX], dy[atoms_MAX];
// number of boxes
int atoms_cnt = atoms_MAX;
// container upper boundary
short int container_y = 0;

volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
volatile int pixel_buffer_start; 
volatile int * HEX3to0 = (int *)0xFF200020;
volatile int * HEX5to4 = (int *)0xFF200030;

short int coef_selected = 0;
int HEXpattern_num[10] = {0x3f,0x06,0x5b,0x4f,0x66,
						  0x6d,0x7d,0x07,0x7f,0x67};
int HEXpattern_P = 0x00007348;
int HEXpattern_V = 0x00003E48;
int HEXpattern_T = 0x00007848;
int HEXpattern_N = 0x00003748;
// functions for drawing
void clear_screen();
void draw_line(int x1, int y1, int x2, int y2, short int line_color);
void draw_square(int x, int y, short int half_side_length, short int line_color);
void draw_pixel(int x, int y, short int line_color);
bool check_collision(int x1, int y1, int x2, int y2, short int speed);
void wait_for_vsync();
// functions for interrupts
void disable_A9_interrupts (void);
void set_A9_IRQ_stack (void);
void config_GIC (void);
void config_KEYs (void);
void enable_A9_interrupts (void);
void pushbutton_ISR (void);
void config_interrupt (int, int);
int decimal_to_HEXpattern(int number);
	
int main(void)
{
	disable_A9_interrupts (); // disable interrupts in the A9 processor
	set_A9_IRQ_stack (); // initialize the stack pointer for IRQ mode
	config_GIC (); // configure the general interrupt controller
	config_KEYs (); // configure KEYs to generate interrupts
	enable_A9_interrupts (); // enable interrupts in the A9 processor
	
    // declare variables:
	// coefficient that is displayed on HEX
	selected_coef = 0;
	// pressure in Pa
	pressure = 498;
	// volume in m^3
	volume = 100;
	// temperature in K
	temperature = 300; 
	// speed of boxes in pixel per frame
	speed = 3;
	// position of previous atoms
	int x_prev[atoms_MAX]={0}, y_prev[atoms_MAX]={0};
	// colours and box colours
	short int colours[atoms_MAX] = {0xF800, 0x07E0, 0x001F, 0x06DC, 
								 0xBFF7, 0xFDC0, 0xFB39 , 0xFFFF};
	short int box_colours[atoms_MAX];
	// initialize location and velocities of rectangles:
	srand (time(NULL));
	for (int i = 0; i < atoms_cnt; ++i){	
		// set x randomly from 0 to 319
		x[i] = rand() % 320;
		// set y randomly from 0 to 239
		y[i] = rand() % (240 - container_y) + container_y;
		// set x direction randomly as (-1 or 1) * speed
		dx[i] = (rand() % 2 * 2 - 1) * speed;
		// set y direction randomly as (-1 or 1) * speed
		dy[i] = (rand() % 2 * 2 - 1) * speed;
		// set box_colours randomly
		box_colours[i] = colours[i%8];
	}
	/*
	x[0] = 1; y[0] = 1;
	dx[0] = 1; dy[0] = 1;
	
	x[1] = 318; y[1] = 1;
	dx[1] = -1; dy[1] = 1;
	*/
	*HEX5to4 = HEXpattern_P;
	*HEX3to0 = decimal_to_HEXpattern(pressure);
	
    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // store in back buffer first
    wait_for_vsync();					// swap
    /* clear front buffer data */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen();
	
	/* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
	/* clear back buffer data */
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
	clear_screen();
	
    while (1)
    {
        // Erase any boxes and lines that were drawn in the last iteration
		for (int i = 0; i < atoms_cnt; ++i){
			draw_square(x_prev[i],y_prev[i],1,0);
        }
		for (int i = 0; i < atoms_cnt; ++i){
			x_prev[i] = x[i];
			y_prev[i] = y[i];
		}
		
		// updating dx dy due to collision
		for (int i = 0; i < atoms_cnt; ++i){
			for (int j = 0; i > j; ++j){
				// check collision
				if (check_collision(x[i],y[i],x[j],y[j],speed)){
					// swap dx
					int temp = dx[i];
					dx[i] = dx[j];
					dx[j] = temp;
					// swap dy
					temp = dy[i];
					dy[i] = dy[j];
					dy[j] = temp;
				}
			}
		}
		
		// updating the directions hitting boundary and locations
		for (int i = 0; i < atoms_cnt; ++i){		
			// updating dx due to boundary
			if (x[i] >= 319){
				dx[i] = - speed;
			}else if (x[i] <= 0){
				dx[i] = speed;
			}
			// updating dy due to boundary
			if (y[i] >= 239){
				dy[i] = - speed;
			}else if (y[i] <= container_y){
				dy[i] = speed;
			}
			// updating x
			x[i] = x[i] + dx[i];
			// updating y
			y[i] = y[i] + dy[i];
        }
		
		// code for drawing the boxes	
		for (int i = 0; i < atoms_cnt; ++i){
			draw_square(x[i],y[i],1,box_colours[i]);
			//draw_line(x[i],y[i], x[(i+1)%8], y[(i+1)%8], line_colours[i%8]);
        }
        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
		// code for drawing container bounds
		draw_line(0,container_y-speed,319,container_y-speed,0xFFFF);
    }
}

/*************************functions for drawing*******************************/

void clear_screen(){
	for (int x = 0; x < 320; ++x){
		for(int y = 0; y < 240; ++y){
			draw_pixel(x,y,0x0000);
		}
	}
}

void draw_square(int x, int y, short int half_side_length, short int line_color){
	for (int i = x-half_side_length; 
			i <= x+half_side_length;
			++i){
		for (int j = y-half_side_length; 
				j <= y+half_side_length;
				++j){
			draw_pixel(i,j,line_color);
		}
	}
}

void draw_line(int x0, int y0, int x1, int y1, short int line_color){
	bool is_steep = ( abs(y1 - y0) > abs(x1 - x0) );
	
	if (is_steep){
		int temp;
		// swap(x0, y0)
		temp = x0;
		x0 = y0;
		y0 = temp;
		// swap(x1, y1)
		temp = x1;
		x1 = y1;
		y1 = temp;
	}
	
	if (x0 > x1){
		int temp;
		// swap(x0, x1)
		temp = x0;
		x0 = x1;
		x1 = temp;
		// swap(y0, y1)
		temp = y0;
		y0 = y1;
		y1 = temp;
	}
	
	int deltax = x1 - x0;
	int deltay = abs(y1 - y0);
	int error = -(deltax / 2);
	
	int y_step;
	if (y0 < y1){
		y_step = 1;
	}else{
		y_step = -1;
	}
	
	int y = y0;
	for (int x = x0; x != x1; ++x){
		if (is_steep){
			draw_pixel(y, x, line_color);
		}else{
			draw_pixel(x, y, line_color);
		}
		error = error + deltay;
		if (error >= 0){
			y = y + y_step;
			error = error - deltax;
		}
	}
}

void draw_pixel(int x, int y, short int line_color)
{
	if (x>0 && x<320 && y>0 && y<240){
		*(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
	}
}

void wait_for_vsync(){
	volatile int *pixel_ctrl_ptr = (int *)0xFF203020;
	register int status;
	
	*pixel_ctrl_ptr = 1;
	status = *(pixel_ctrl_ptr + 3);
	while ((status & 0x01) != 0){
		status = *(pixel_ctrl_ptr + 3);
	}
}

bool check_collision(int x1, int y1, int x2, int y2, short int speed){
	return 	(x1 - x2) <= speed && (x1 - x2) >= ((-1)*speed) &&
			(y1 - y2) <= speed && (y1 - y2) >= ((-1)*speed);
}


/*************************functions for interrupts*******************************/

/* setup the KEY interrupts in the FPGA */
void config_KEYs()
{
	volatile int * KEY_ptr = (int *) 0xFF200050; // KEY base address
	*(KEY_ptr + 2) = 0xF; // enable interrupts for all four KEYs
}

/* This file:
 * 1. defines exception vectors for the A9 processor
 * 2. provides code that sets the IRQ mode stack, and that dis/enables
 * interrupts
 * 3. provides code that initializes the generic interrupt controller
 */
// Define the IRQ exception handler
void __attribute__ ((interrupt)) __cs3_isr_irq (void)
{
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *) 0xFFFEC10C);
	if (interrupt_ID == 73) // check if interrupt is from the KEYs
	pushbutton_ISR ();
	else
	while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	*((int *) 0xFFFEC110) = interrupt_ID;
}

// Define the remaining exception handlers
void __attribute__ ((interrupt)) __cs3_reset (void)
{
	while(1);
}

void __attribute__ ((interrupt)) __cs3_isr_undef (void)
{
	while(1);
}

void __attribute__ ((interrupt)) __cs3_isr_swi (void)
{
	while(1);
}

void __attribute__ ((interrupt)) __cs3_isr_pabort (void)
{
	while(1);
}

void __attribute__ ((interrupt)) __cs3_isr_dabort (void)
{
	while(1);
}

void __attribute__ ((interrupt)) __cs3_isr_fiq (void)
{
	while(1);
}

/*
* Turn off interrupts in the ARM processor
*/
void disable_A9_interrupts(void)
{
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

/*
* Initialize the banked stack pointer register for IRQ mode
*/
void set_A9_IRQ_stack(void)
{
	int stack, mode;
	stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = 0b11010010;
	asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r" (stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
}

/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void)
{
	int status = 0b01010011;
	asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void)
{
	config_interrupt (73, 1); // configure the FPGA KEYs interrupt (73)
	// Set Interrupt Priority Mask Register (ICCPMR). Enable all priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set the enable in the CPU Interface Control Register (ICCICR)
	*((int *) 0xFFFEC100) = 1;
	// Set the enable in the Distributor Control Register (ICDDCR)
	*((int *) 0xFFFED000) = 1;
}

/*
* Configure registers in the GIC for an individual Interrupt ID. We
* configure only the Interrupt Set Enable Registers (ICDISERn) and
* Interrupt Processor Target Registers (ICDIPTRn). The default (reset)
* values are used for other registers in the GIC
*/
void config_interrupt (int N, int CPU_target)
{
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4; value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Using the address and value, set the appropriate bit */
	*(int *)address |= value;
	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4; index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Using the address and value, write to (only) the appropriate byte */
	*(char *)address = (char) CPU_target;
}

/********************************************************************
* Pushbutton - Interrupt Service Routine
*
* This routine checks which KEY has been pressed. It writes to HEX0
*******************************************************************/
void pushbutton_ISR( void )
{
	/* KEY base address */
	volatile int *KEY_ptr = (int *) 0xFF200050;
	int press;
	
	press = *(KEY_ptr + 3); // read the pushbutton interrupt register
	*(KEY_ptr + 3) = press; // Clear the interrupt
	// KEY0: reset
	if (press & 0x1){
		// reset coefficients and HEX
		volume = 100;
		temperature = 300;
		pressure = 498;
		if 	(selected_coef == 0){
			pressure = atoms_cnt * gas_constant * temperature / volume;
			*HEX5to4 = HEXpattern_P;
			*HEX3to0 = decimal_to_HEXpattern(pressure);
		}
		else if (selected_coef == 1){
			*HEX5to4 = HEXpattern_V;
			*HEX3to0 = decimal_to_HEXpattern(volume);
		}
		else if (selected_coef == 2){
			*HEX5to4 = HEXpattern_T;
			*HEX3to0 = decimal_to_HEXpattern(temperature);
		}
		else{
			*HEX5to4 = HEXpattern_N;
			*HEX3to0 = decimal_to_HEXpattern(atoms_cnt);
		}
		
		// reset VGA
		container_y = 0;
		speed = 3;
		for (int i = 0; i < atoms_cnt; ++i){	
			// set x randomly from 0 to 319
			x[i] = rand() % 320;
			// set y randomly from 0 to 239
			y[i] = rand() % (240 - container_y) + container_y;
			// set x direction randomly as (-1 or 1) * speed
			dx[i] = (rand() % 2 * 2 - 1) * speed;
			// set y direction randomly as (-1 or 1) * speed
			dy[i] = (rand() % 2 * 2 - 1) * speed;
		}
		
		// clear screen from front buffer
		pixel_buffer_start = *pixel_ctrl_ptr;
		clear_screen();
		// clear screen from back buffer
		pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
		clear_screen();
	}
	// KEY1
	else if (press & 0x2){
		// reset coefficients and HEX
		if 	(selected_coef == 0){
			;
		}
		else if (selected_coef == 1){
			if (volume != 10){
				volume = volume - 10;
				container_y = (1 - (volume / 100.0)) * 240;
				*HEX3to0 = decimal_to_HEXpattern(volume);
				for (int i = 0; i < atoms_cnt; ++i){	
					// set x randomly from 0 to 319
					x[i] = rand() % 320;
					// set y randomly from 0 to 239
					y[i] = rand() % (240 - container_y) + container_y;
					// set x direction randomly as (-1 or 1) * speed
					dx[i] = (rand() % 2 * 2 - 1) * speed;
					// set y direction randomly as (-1 or 1) * speed
					dy[i] = (rand() % 2 * 2 - 1) * speed;
				}			
				/* set front pixel buffer to start of FPGA On-chip memory */
				*(pixel_ctrl_ptr + 1) = 0xC8000000; // store in back buffer first
				wait_for_vsync();					// swap
				/* clear front buffer data */
				pixel_buffer_start = *pixel_ctrl_ptr;
				clear_screen();
				/* set back pixel buffer to start of SDRAM memory */
				*(pixel_ctrl_ptr + 1) = 0xC0000000;
				/* clear back buffer data */
				pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
				clear_screen();
			}else{
				*HEX3to0 = decimal_to_HEXpattern(10);
			}
		}
		else if (selected_coef == 2){
			temperature = (temperature == 100) ? 100 : temperature-100;
			// clear screen from front buffer
			pixel_buffer_start = *pixel_ctrl_ptr;
			draw_line(0,container_y - speed,319,container_y - speed,0);
			// clear screen from back buffer
			pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
			draw_line(0,container_y - speed,319,container_y - speed,0);
			// update speed
			speed = temperature / 100;
			for (int i = 0; i < atoms_cnt; ++i){	
				// update dx
				dx[i] = dx[i] / abs(dx[i]) * speed;
				// update dy
				dy[i] = dy[i] / abs(dy[i]) * speed;
			}
			*HEX3to0 = decimal_to_HEXpattern(temperature);
		}
		else{
			if (atoms_cnt != 0){
				atoms_cnt = atoms_cnt - 1;
				*HEX3to0 = decimal_to_HEXpattern(atoms_cnt);
				
				// clear screen from front buffer
				pixel_buffer_start = *pixel_ctrl_ptr;
				clear_screen();
				// clear screen from back buffer
				pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
				clear_screen();
			}else{
				*HEX3to0 = decimal_to_HEXpattern(0);
			}
		}
	}
	// KEY2
	else if (press & 0x4){
		if 	(selected_coef == 0){
			;
		}
		else if (selected_coef == 1){
			if (volume != 100){
				volume = volume + 10;
				container_y = (1 - (volume / 100.0)) * 240;
				*HEX3to0 = decimal_to_HEXpattern(volume);
				
				/* set front pixel buffer to start of FPGA On-chip memory */
				*(pixel_ctrl_ptr + 1) = 0xC8000000; // store in back buffer first
				wait_for_vsync();					// swap
				/* clear front buffer data */
				pixel_buffer_start = *pixel_ctrl_ptr;
				clear_screen();
				/* set back pixel buffer to start of SDRAM memory */
				*(pixel_ctrl_ptr + 1) = 0xC0000000;
				/* clear back buffer data */
				pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
				clear_screen();
			}else{
				*HEX3to0 = decimal_to_HEXpattern(100);
			}
		}
		else if (selected_coef == 2){
			temperature = (temperature == 600) ? 600 : temperature+100;
			// clear screen from front buffer
			pixel_buffer_start = *pixel_ctrl_ptr;
			draw_line(0,container_y - speed,319,container_y - speed,0);
			// clear screen from back buffer
			pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
			draw_line(0,container_y - speed,319,container_y - speed,0);
			// update speed
			speed = temperature / 100;
			for (int i = 0; i < atoms_cnt; ++i){	
				// update dx
				dx[i] = dx[i] / abs(dx[i]) * speed;
				// update dy
				dy[i] = dy[i] / abs(dy[i]) * speed;
			}
			*HEX3to0 = decimal_to_HEXpattern(temperature);
		}
		else{
			if (atoms_cnt != 20){
				atoms_cnt = atoms_cnt + 1;
				*HEX3to0 = decimal_to_HEXpattern(atoms_cnt);
				
				// clear screen from front buffer
				pixel_buffer_start = *pixel_ctrl_ptr;
				clear_screen();
				// clear screen from back buffer
				pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
				clear_screen();
			}else{
				*HEX3to0 = decimal_to_HEXpattern(20);
			}
		}
	}
	// KEY3
	else{
		selected_coef = (selected_coef + 1) % 4;
		if 		(selected_coef == 0){
			pressure = atoms_cnt * gas_constant * temperature / volume;
			*HEX5to4 = HEXpattern_P;
			*HEX3to0 = decimal_to_HEXpattern(pressure);
		}
		else if (selected_coef == 1){
			*HEX5to4 = HEXpattern_V;
			*HEX3to0 = decimal_to_HEXpattern(volume);
		}
		else if (selected_coef == 2){
			*HEX5to4 = HEXpattern_T;
			*HEX3to0 = decimal_to_HEXpattern(temperature);
		}
		else{
			*HEX5to4 = HEXpattern_N;
			*HEX3to0 = decimal_to_HEXpattern(atoms_cnt);
		}
	}
	return;
}

int decimal_to_HEXpattern(int number){
	int pattern = 0;
	int bit = 0;
	do{
		int remainder = number%10;
		number    = number/10;
		pattern += (HEXpattern_num[remainder] << (bit * 8));
		++bit;
	}while (number != 0);
	return pattern;
}