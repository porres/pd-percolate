/******************************************/
/*  Waveguide Brass Instrument Model ala  */
/*  Cook (TBone, HosePlayer)              */
/*  by Perry R. Cook, 1995-96             */
/*										  */
/*  ported to MSP by Dan Trueman, 2000	  */
/*                                        */
/*  ported to PD by Olaf Matthes, 2002	  */
/*                                        */
/*  This is a waveguide model, and thus   */
/*  relates to various Stanford Univ.     */
/*  and possibly Yamaha and other patents.*/
/*                                        */
/*   Controls:    CONTROL1 = lipTension   */
/*                CONTROL2 = slideLength  */
/*		  		  CONTROL3 = vibFreq      */
/*		  		  MOD_WHEEL= vibAmt       */
/******************************************/

#include "stk.h"
#include <math.h> 
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#ifdef PD
#include "m_pd.h"
#endif

#define LENGTH 22050	//44100/LOWFREQ + 1 --brass length
#define VIBLENGTH 1024

/* ------------------------------- MSP ----------------------------------------- */
#ifdef MSP

void *brass_class;

typedef struct _brass
{
	//header
    t_pxobject x_obj;
    
    //user controlled vars
    float lipTension;
    float slideTargetMult;
    float vibrGain;
    float maxPressure;
    float x_vf; 		//vib freq
    float x_fr;			//frequency	
    
    float lipTension_save;
    float slideTargetMult_save;
    float fr_save;
    
    //signals connected? or controls...
    short x_ltconnected;
    short x_stconnected;
    short x_vgconnected;
    short x_mpconnected;
    short x_vfconnected;
    short x_frconnected;
    
    //delay line
    DLineA delayLine;
    
    //lip filter
    LipFilt lipFilter;
    
    //DC blocker
    DCBlock killdc;
    
    //vibrato table
    float vibTable[VIBLENGTH];
    float vibRate;
    float vibTime;
    
    //stuff
    float lipTarget, slideTarget;
    float srate, one_over_srate;
} t_brass;

/****PROTOTYPES****/

//setup funcs
void *brass_new(double val);
void brass_free(t_brass *x);
void brass_dsp(t_brass *x, t_signal **sp, short *count);
void brass_float(t_brass *x, double f);
t_int *brass_perform(t_int *w);
void brass_assist(t_brass *x, void *b, long m, long a, char *s);

//vib funcs
void setVibFreq(t_brass *x, float freq);
float vib_tick(t_brass *x);

//brass functions
void setFreq(t_brass *x, float frequency);


/****FUNCTIONS****/

void setFreq(t_brass *x, float frequency)
{
  if(frequency < 20.) frequency = 20.;
  x->slideTarget = (x->srate / frequency * 2.0) + 3.0;
  /* fudge correction for filter delays */
  DLineA_setDelay(&x->delayLine, x->slideTarget);	/*  we'll play a harmonic  */
  x->lipTarget = frequency;
  LipFilt_setFreq(&x->lipFilter, frequency, x->srate);
}

//vib funcs
void setVibFreq(t_brass *x, float freq)
{
	x->vibRate = VIBLENGTH * x->one_over_srate * freq;
}

float vib_tick(t_brass *x)
{
	long temp;
	float temp_time, alpha, output;
	
	x->vibTime += x->vibRate;
	while (x->vibTime >= (float)VIBLENGTH) x->vibTime -= (float)VIBLENGTH;
	while (x->vibTime < 0.) x->vibTime += (float)VIBLENGTH;
	
	temp_time = x->vibTime;
	
	temp = (long) temp_time;
	alpha = temp_time - (float)temp;
	output = x->vibTable[temp];
	output = output + (alpha * (x->vibTable[temp+1] - output));
	return output;
}

//primary MSP funcs
void main(void)
{
    setup((struct messlist **)&brass_class, (method)brass_new, (method)brass_free, (short)sizeof(t_brass), 0L, A_DEFFLOAT, 0);
    addmess((method)brass_dsp, "dsp", A_CANT, 0);
    addmess((method)brass_assist,"assist",A_CANT,0);
    addfloat((method)brass_float);
    dsp_initclass();
    rescopy('STR#',98021);
}

void brass_assist(t_brass *x, void *b, long m, long a, char *s)
{
	assist_string(98021,m,a,1,7,s);
}

void brass_float(t_brass *x, double f)
{
	if (x->x_obj.z_in == 0) {
		x->lipTension = f;
	} else if (x->x_obj.z_in == 1) {
		x->slideTargetMult = f;
	} else if (x->x_obj.z_in == 2) {
		x->vibrGain = f;
	} else if (x->x_obj.z_in == 3) {
		x->maxPressure = f;
	} else if (x->x_obj.z_in == 4) {
		x->x_vf = f;
	} else if (x->x_obj.z_in == 5) {
		x->x_fr = f;
	}
}

void *brass_new(double initial_coeff)
{
	int i;

    t_brass *x = (t_brass *)newobject(brass_class);
     //zero out the struct, to be careful (takk to jkclayton)
    if (x) { 
        for(i=sizeof(t_pxobject)-1;i<sizeof(t_brass);i++)  
                ((char *)x)[i]=0; 
	} 
    dsp_setup((t_pxobject *)x,6);
    outlet_new((t_object *)x, "signal");
    x->lipTension 			= 0.5;
    x->lipTension_save 		= 0.5;
    x->slideTargetMult 		= 0.5;
    x->slideTargetMult_save = 0.5;
    x->vibrGain 			= 0.5;
    x->maxPressure 			= 0.05;
    x->lipTarget  = x->x_fr = 440.;
    x->x_vf = 5.;
    
    x->srate = sys_getsr();
    x->one_over_srate = 1./x->srate;
    
    DLineA_alloc(&x->delayLine, LENGTH);
    
    for(i=0; i<VIBLENGTH; i++) x->vibTable[i] = sin(i*TWO_PI/VIBLENGTH);
    x->vibRate = 1.;
    x->vibTime = 0.;
    
    //clear stuff
    DLineA_clear(&x->delayLine);
    LipFilt_init(&x->lipFilter);
    
    //initialize things
    DLineA_setDelay(&x->delayLine, 100.);

    setFreq(x, x->x_fr);
    setVibFreq(x, 5.925);

    x->fr_save = x->x_fr;
    
    post("what exactly is that sound?");
    
    return (x);
}

void brass_free(t_brass *x)
{
	DLineA_free(&x->delayLine);
	dsp_free((t_pxobject *)x);
}

void brass_dsp(t_brass *x, t_signal **sp, short *count)
{
	x->x_ltconnected = count[0];
	x->x_stconnected = count[1];
	x->x_vgconnected = count[2];
	x->x_mpconnected = count[3];
	x->x_vfconnected = count[4];
	x->x_frconnected = count[5];
	x->srate = sp[0]->s_sr;
    x->one_over_srate = 1./x->srate;
	dsp_add(brass_perform, 9, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[0]->s_n);	
	
}

t_int *brass_perform(t_int *w)
{
	t_brass *x = (t_brass *)(w[1]);

	float lipTension 		= x->x_ltconnected? *(float *)(w[2]) : x->lipTension;
	float slideTargetMult 	= x->x_stconnected? *(float *)(w[3]) : x->slideTargetMult;
	float vibrGain 			= x->x_vgconnected? *(float *)(w[4]) : x->vibrGain;
	float maxPressure		= x->x_mpconnected? *(float *)(w[5]) : x->maxPressure;
	float vf 				= x->x_vfconnected? *(float *)(w[6]) : x->x_vf;
	float fr 				= x->x_frconnected? *(float *)(w[7]) : x->x_fr;
	
	float *out = (float *)(w[8]);
	long n = w[9];
	
	float temp, breathPressure;	

	if(fr != x->fr_save) {
		if(fr < 20.) fr = 20.;
  		x->slideTarget = (x->srate / fr * 2.0) + 3.0;
  		x->lipTarget = fr;
		LipFilt_setFreq(&x->lipFilter, x->lipTarget*lipTension, x->srate);
		DLineA_setDelay(&x->delayLine, x->slideTarget*slideTargetMult);
		x->fr_save = fr;
	}
	
    if(lipTension != x->lipTension_save) {
    	LipFilt_setFreq(&x->lipFilter, x->lipTarget*lipTension, x->srate);
    	x->lipTension_save = lipTension;
    }
    
    if(slideTargetMult != x->slideTargetMult_save) {
   		DLineA_setDelay(&x->delayLine, x->slideTarget*slideTargetMult);
   		x->slideTargetMult_save = slideTargetMult;
   	}
	
	x->vibRate = VIBLENGTH * x->one_over_srate * vf; 

	while(n--) {
		breathPressure = maxPressure;
  		breathPressure += vibrGain * vib_tick(x);
  		/* mouth input and bore reflection */
  		temp = LipFilt_tick(&x->lipFilter, .3*breathPressure, .85*x->delayLine.lastOutput);
  		temp = DCBlock_tick(&x->killdc, temp);			/* block DC    */
  		*out++ = DLineA_tick(&x->delayLine, temp);		/* bore delay  */
	}
	return w + 10;
}	
#endif /* MSP */

/* --------------------------------- Pure Data ------------------------------- */
#ifdef PD
static t_class *brass_class;

typedef struct _brass
{
	//header
    t_object x_obj;
    
    //user controlled vars
    float lipTension;
    float slideTargetMult;
    float vibrGain;
    float maxPressure;
    float x_vf; 		//vib freq
    float x_fr;			//frequency	
    
    float lipTension_save;
    float slideTargetMult_save;
    float fr_save;
    
    //delay line
    DLineA delayLine;
    
    //lip filter
    LipFilt lipFilter;
    
    //DC blocker
    DCBlock killdc;
    
    //vibrato table
    float vibTable[VIBLENGTH];
    float vibRate;
    float vibTime;
    
    //stuff
    float lipTarget, slideTarget;
    float srate, one_over_srate;
} t_brass;

/****FUNCTIONS****/

static void setFreq(t_brass *x, float frequency)
{
  if(frequency < 20.) frequency = 20.;
  x->slideTarget = (x->srate / frequency * 2.0) + 3.0;
  /* fudge correction for filter delays */
  DLineA_setDelay(&x->delayLine, x->slideTarget);	/*  we'll play a harmonic  */
  x->lipTarget = frequency;
  LipFilt_setFreq(&x->lipFilter, frequency, x->srate);
}

//vib funcs
static void setVibFreq(t_brass *x, float freq)
{
	x->vibRate = VIBLENGTH * x->one_over_srate * freq;
}

static float vib_tick(t_brass *x)
{
	long temp;
	float temp_time, alpha, output;
	
	x->vibTime += x->vibRate;
	while (x->vibTime >= (float)VIBLENGTH) x->vibTime -= (float)VIBLENGTH;
	while (x->vibTime < 0.) x->vibTime += (float)VIBLENGTH;
	
	temp_time = x->vibTime;
	
	temp = (long) temp_time;
	alpha = temp_time - (float)temp;
	output = x->vibTable[temp];
	output = output + (alpha * (x->vibTable[temp+1] - output));
	return output;
}

static t_int *brass_perform(t_int *w)
{
	t_brass *x = (t_brass *)(w[1]);

	float lipTension 		= x->lipTension;
	float slideTargetMult 	= x->slideTargetMult;
	float vibrGain 			= x->vibrGain;
	float maxPressure		= x->maxPressure;
	float vf 				= x->x_vf;
	float fr 				= x->x_fr;
	
	t_float *out = (float *)(w[2]);
	long n = w[3];
	
	float temp, breathPressure;	

	if(fr != x->fr_save) {
		if(fr < 20.) fr = 20.;
  		x->slideTarget = (x->srate / fr * 2.0) + 3.0;
  		x->lipTarget = fr;
		LipFilt_setFreq(&x->lipFilter, x->lipTarget*lipTension, x->srate);
		DLineA_setDelay(&x->delayLine, x->slideTarget*slideTargetMult);
		x->fr_save = fr;
	}
	
    if(lipTension != x->lipTension_save) {
    	LipFilt_setFreq(&x->lipFilter, x->lipTarget*lipTension, x->srate);
    	x->lipTension_save = lipTension;
    }
    
    if(slideTargetMult != x->slideTargetMult_save) {
   		DLineA_setDelay(&x->delayLine, x->slideTarget*slideTargetMult);
   		x->slideTargetMult_save = slideTargetMult;
   	}
	
	x->vibRate = VIBLENGTH * x->one_over_srate * vf; 

	while(n--) {
		breathPressure = maxPressure;
  		breathPressure += vibrGain * vib_tick(x);
  		/* mouth input and bore reflection */
  		temp = LipFilt_tick(&x->lipFilter, .3*breathPressure, .85*x->delayLine.lastOutput);
  		temp = DCBlock_tick(&x->killdc, temp);			/* block DC    */
  		*out++ = DLineA_tick(&x->delayLine, temp);		/* bore delay  */
	}
	return w + 4;
}	

static void brass_dsp(t_brass *x, t_signal **sp)
{
	x->srate = sp[0]->s_sr;
    x->one_over_srate = 1./x->srate;
	dsp_add(brass_perform, 3, x, sp[1]->s_vec, sp[0]->s_n);	
	
}




static void brass_float(t_brass *x, t_floatarg f)
{
		x->lipTension = f;
}

static void brass_slideTargetMult(t_brass *x, t_floatarg f)
{
	x->slideTargetMult = f;
}

		
static void brass_vibrGain(t_brass *x, t_floatarg f)
{
	x->vibrGain = f;
}

		
static void brass_vf(t_brass *x, t_floatarg f)
{
	x->x_vf = f;
}

		
static void brass_maxPressure(t_brass *x, t_floatarg f)
{
	x->maxPressure = f;
}

		
static void brass_freq(t_brass *x, t_floatarg f)
{
	x->x_fr = f;
}

static void brass_free(t_brass *x)
{
	DLineA_free(&x->delayLine);
}

static void *brass_new(void)
{
	unsigned int i;

    t_brass *x = (t_brass *)pd_new(brass_class);
     //zero out the struct, to be careful (takk to jkclayton)
    if (x) { 
        for(i=sizeof(t_object)-1;i<sizeof(t_brass);i++)  
                ((char *)x)[i]=0; 
	} 
    outlet_new(&x->x_obj, gensym("signal"));

	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("slideTargetMult"));
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("vibrGain"));
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("maxPressure"));
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("vf"));
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("freq"));

    x->lipTension 			= 0.5;
    x->lipTension_save 		= 0.5;
    x->slideTargetMult 		= 0.5;
    x->slideTargetMult_save = 0.5;
    x->vibrGain 			= 0.5;
    x->maxPressure 			= 0.05;
    x->lipTarget  = x->x_fr = 440.;
    x->x_vf = 5.;
    
    x->srate = sys_getsr();
    x->one_over_srate = 1./x->srate;
    
    DLineA_alloc(&x->delayLine, LENGTH);
    
    for(i=0; i<VIBLENGTH; i++) x->vibTable[i] = sin(i*TWO_PI/VIBLENGTH);
    x->vibRate = 1.;
    x->vibTime = 0.;
    
    //clear stuff
    DLineA_clear(&x->delayLine);
    LipFilt_init(&x->lipFilter);
    
    //initialize things
    DLineA_setDelay(&x->delayLine, 100.);

    setFreq(x, x->x_fr);
    setVibFreq(x, 5.925);

    x->fr_save = x->x_fr;
    
    post("what exactly is that sound?");
    
    return (x);
}

void brass_tilde_setup(void)
{
    brass_class = class_new(gensym("brass~"), (t_newmethod)brass_new, (t_method)brass_free,
        sizeof(t_brass), 0, 0);
    class_addmethod(brass_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(brass_class, (t_method)brass_dsp, gensym("dsp"), A_NULL);
	class_addfloat(brass_class, (t_method)brass_float);
    class_addmethod(brass_class, (t_method)brass_maxPressure, gensym("maxPressure"), A_FLOAT, A_NULL);
    class_addmethod(brass_class, (t_method)brass_slideTargetMult, gensym("slideTargetMult"), A_FLOAT, A_NULL);
    class_addmethod(brass_class, (t_method)brass_vf, gensym("vf"), A_FLOAT, A_NULL);
    class_addmethod(brass_class, (t_method)brass_vibrGain, gensym("vibrGain"), A_FLOAT, A_NULL);
    class_addmethod(brass_class, (t_method)brass_freq, gensym("freq"), A_FLOAT, A_NULL);
}
#endif /* PD */
