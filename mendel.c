#include	<stddef.h>
#include	<stdint.h>
#include	<string.h>

#include	<avr/io.h>
#include	<avr/interrupt.h>

#include	"machine.h"

#include	"serial.h"
#include	"dda_queue.h"
#include	"dda.h"
#include	"gcode.h"
#include	"timer.h"
#include	"clock.h"
#include	"temp.h"
#include	"sermsg.h"
#include	"watchdog.h"
#include	"debug.h"
#include	"intercom.h"
#include	"sersendf.h"

void io_init(void) {
	// disable modules we don't use
	#ifdef PRR
	PRR = MASK(PRTWI) | MASK(PRADC);
	#else
	PRR0 = MASK(PRTWI) | MASK(PRADC);
	PRR1 = 0xFF;
	#endif
	ACSR = MASK(ACD);

	// setup I/O pins
	WRITE(DEBUG_LED, 0);	SET_OUTPUT(DEBUG_LED);

	WRITE(X_STEP_PIN, 0);	SET_OUTPUT(X_STEP_PIN);
	WRITE(X_DIR_PIN,  0);	SET_OUTPUT(X_DIR_PIN);
	WRITE(X_MIN_PIN,  1);	SET_INPUT(X_MIN_PIN);

	WRITE(Y_STEP_PIN, 0);	SET_OUTPUT(Y_STEP_PIN);
	WRITE(Y_DIR_PIN,  0);	SET_OUTPUT(Y_DIR_PIN);
	WRITE(Y_MIN_PIN,  1);	SET_INPUT(Y_MIN_PIN);

	WRITE(Z_STEP_PIN, 0);	SET_OUTPUT(Z_STEP_PIN);
	WRITE(Z_DIR_PIN,  0);	SET_OUTPUT(Z_DIR_PIN);
	WRITE(Z_MIN_PIN,  1);	SET_INPUT(Z_MIN_PIN);

	WRITE(E_STEP_PIN, 0);	SET_OUTPUT(E_STEP_PIN);
	WRITE(E_DIR_PIN,  0);	SET_OUTPUT(E_DIR_PIN);

	//Enable the RS485 transceiver
	SET_OUTPUT(RX_ENABLE_PIN);
	SET_OUTPUT(TX_ENABLE_PIN);
	disable_transmit();

	SET_OUTPUT(TX_485_PIN);
	SET_INPUT(RX_485_PIN);


	#ifdef	HEATER_PIN
		WRITE(HEATER_PIN, 0); SET_OUTPUT(HEATER_PIN);
	#endif

	#ifdef	FAN_PIN
		WRITE(FAN_PIN, 0); SET_OUTPUT(FAN_PIN);
	#endif

	#if defined(HEATER_PWM) || defined(FAN_PWM)
		// setup PWM timer: fast PWM, no prescaler
		TCCR0A = MASK(WGM01) | MASK(WGM00);
		TCCR0B = MASK(CS00);
		TIMSK0 = 0;
		OCR0A = 0;
		OCR0B = 255;
	#endif

	#ifdef	STEPPER_ENABLE_PIN
		power_off();
	#endif

}

void init(void) {

	// set up watchdog
	wd_init();

	// set up serial
	serial_init();

	// set up intercom
	intercom_init();

	// set up inputs and outputs
	io_init();

	// set up timers
	setupTimerInterrupt();

	// set up clock
	clock_setup();

	// read PID settings from EEPROM
	temp_init();

	// set up default feedrate
	current_position.F = startpoint.F = next_target.target.F = SEARCH_FEEDRATE_Z;

	// enable interrupts
	sei();

	// reset watchdog
	wd_reset();

	// say hi to host
	serial_writestr_P(PSTR("Start\nOK\n"));

}

void clock_250ms(void) {
	// reset watchdog
	wd_reset();

	
	sersendf_P(PSTR("CMD: %d\n"), get_read_cmd());

	temp_tick();

	if (steptimeout > (30 * 4)) {
		power_off();
	}
	else
		steptimeout++;

	ifclock(CLOCK_FLAG_1S) {
		if (debug_flags & DEBUG_POSITION) {
			// current position
			sersendf_P(PSTR("Pos: %ld,%ld,%ld,%ld,%lu\n"), current_position.X, current_position.Y, current_position.Z, current_position.E, current_position.F);

			// target position
			sersendf_P(PSTR("Dst: %ld,%ld,%ld,%ld,%lu\n"), movebuffer[mb_tail].endpoint.X, movebuffer[mb_tail].endpoint.Y, movebuffer[mb_tail].endpoint.Z, movebuffer[mb_tail].endpoint.E, movebuffer[mb_tail].endpoint.F);

			// Queue
			print_queue();
		}
		// temperature
		if (temp_get_target())
			temp_print();
	}
}

int main (void)
{
	init();

	sersendf_P(PSTR("Hello Mendel"));

	update_send_cmd(0);

	start_send();

	// main loop
	for (;;)
	{
		// if queue is full, no point in reading chars- host will just have to wait
		if ((serial_rxchars() != 0) && (queue_full() == 0)) {
			uint8_t c = serial_popchar();
			scan_char(c);
		}

		ifclock(CLOCK_FLAG_250MS) {
			clock_250ms();
		}
	}
}