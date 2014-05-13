#ifndef __LIB_RUN_ZO3_H
	#define __LIB_RUN_ZO3_H
	
	#define SRV_RUN_CLIENT	"10.242.7.2"
	#define SRV_RUN_PORT	45871
	
	typedef enum action_run_lang_t {
		C,
		PYTHON,
		HASKELL,
		PHP,
		
	} action_run_lang_t;
	
	void lib_run_init(ircmessage_t *message, char *code, action_run_lang_t lang);
#endif
