#ifndef __Z03_LIB_REGISTRAR_H
	#define __Z03_LIB_REGISTRAR_H
	
	// register
	void request_register(request_t *request);
	
	#ifndef __LIB_CORE_C
		#define __registrar  __attribute__ ((constructor)) void
	#endif
#endif
