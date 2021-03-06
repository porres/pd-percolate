// threshhold-based wraparound thingy.
// by r. luke dubois, cmc/cu, 2000.

#include <math.h>
#ifdef MSP
#include "ext.h"
#include "z_dsp.h"
#endif
#ifdef PD
#include "m_pd.h"
#endif
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#ifdef MSP
void *sigflip_class;

typedef struct _sigflip
{
    t_pxobject x_obj;
    t_float x_val;
} t_sigflip;

void *sigflip_new(double val);
t_int *offset_perform(t_int *w);
t_int *sigflip2_perform(t_int *w);
void sigflip_float(t_sigflip *x, double f);
void sigflip_int(t_sigflip *x, long n);
void sigflip_dsp(t_sigflip *x, t_signal **sp, short *count);
void sigflip_assist(t_sigflip *x, void *b, long m, long a, char *s);

void main(void)
{
    setup(&sigflip_class, sigflip_new, (method)dsp_free, (short)sizeof(t_sigflip), 0L, A_DEFFLOAT, 0);
    addmess((method)sigflip_dsp, "dsp", A_CANT, 0);
    addfloat((method)sigflip_float);
    addint((method)sigflip_int);
    addmess((method)sigflip_assist,"assist",A_CANT,0);
    dsp_initclass();
    
    post("flip~: by r. luke dubois, cmc");
}

void sigflip_assist(t_sigflip *x, void *b, long m, long a, char *s)
{
	switch(m) {
		case 1: // inlet
			switch(a) {
				case 0:
				sprintf(s, "(Signal) Input");
				break;
				case 1:
				sprintf(s, "(Signal) Wraparound Threshhold");
				break;
			}
		break;
		case 2: // outlet
			switch(a) {
				case 0:
				sprintf(s, "(Signal) Output");
				break;
			}
		break;
	}
}

void *sigflip_new(double val)
{
    t_sigflip *x = (t_sigflip *)newobject(sigflip_class);
    dsp_setup((t_pxobject *)x,2);
    outlet_new((t_pxobject *)x, "signal");
    x->x_val = val;
    return (x);
}

// this routine covers both inlets. It doesn't matter which one is involved

void sigflip_float(t_sigflip *x, double f)
{
	x->x_val = f;
}

void sigflip_int(t_sigflip *x, long n)
{
	x->x_val = (float)n;
}

t_int *offset_perform(t_int *w)
{
    t_float *in = (t_float *)(w[1]);
    t_float *out = (t_float *)(w[2]);
	t_sigflip *x = (t_sigflip *)(w[3]);
	float val = fabs(x->x_val);
	float inval;
	int n = (int)(w[4]);
	
	if (x->x_obj.z_disabled)
		goto out;
	
	while (--n) {
		inval = *++in;
    	if (fabs(inval)>val) {
    		if(inval>=0) {
	    		*++out = val-(inval-val);
	    	}
	    	else {
	    		*++out = (0.-val)+((fabs(inval))-val);
	    	}
    	}
		else {
    		*++out = inval;
    	}
	}
    
out:
    return (w+5);
}

t_int *sigflip2_perform(t_int *w)
{
	t_float *in1,*in2,*out;
	int n;
	float inval, val;

	if (*(long *)(w[1]))
	    goto out;

	in1 = (t_float *)(w[2]);
	in2 = (t_float *)(w[3]);
	out = (t_float *)(w[4]);
	n = (int)(w[5]);
	while (--n) {
		inval = *++in1;
		val = fabs(*++in2);
    	if (fabs(inval)>val) {
    		if(inval>=0) {
	    		*++out = val-(inval-val);
	    	}
	    	else {
	    		*++out = (0.-val)+((fabs(inval))-val);
	    	}
    	}
		else {
    		*++out = inval;
    	}
	}
out:
	return (w+6);
}		

void sigflip_dsp(t_sigflip *x, t_signal **sp, short *count)
{
	long i;
		
	if (!count[0])
		dsp_add(offset_perform, 4, sp[1]->s_vec-1, sp[2]->s_vec-1, x, sp[0]->s_n+1);
	else if (!count[1])
		dsp_add(offset_perform, 4, sp[0]->s_vec-1, sp[2]->s_vec-1, x, sp[0]->s_n+1);
	else
		dsp_add(sigflip2_perform, 5, &x->x_obj.z_disabled, sp[0]->s_vec-1, sp[1]->s_vec-1, sp[2]->s_vec-1, sp[0]->s_n+1);
}
#endif /* MSP */


/* ------------------------------------------- Pure data ------------------------------------------ */
#ifdef PD
static t_class *sigflip_class;

typedef struct _sigflip
{
    t_object x_obj;
    t_float x_val;
} t_sigflip;


static t_int *sigflip2_perform(t_int *w)
{
	t_float *in1,*in2,*out;
	int n;
	float inval, val;

	in1 = (t_float *)(w[1]);
	in2 = (t_float *)(w[2]);
	out = (t_float *)(w[3]);
	n = (int)(w[4]);
	while (--n) {
		inval = *++in1;
		val = fabs(*++in2);
    	if (fabs(inval)>val) {
    		if(inval>=0) {
	    		*++out = val-(inval-val);
	    	}
	    	else {
	    		*++out = (0.-val)+((fabs(inval))-val);
	    	}
    	}
		else {
    		*++out = inval;
    	}
	}
	return (w+5);
}		

static void sigflip_dsp(t_sigflip *x, t_signal **sp)
{
	dsp_add(sigflip2_perform, 4, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}


static void sigflip_float(t_sigflip *x, t_floatarg f)
{
	x->x_val = f;
}

static void *sigflip_new(t_floatarg val)
{
    t_sigflip *x = (t_sigflip *)pd_new(sigflip_class);

    outlet_new(&x->x_obj, gensym("signal"));
	inlet_new (&x->x_obj, &x->x_obj.ob_pd, gensym ("signal"), gensym ("signal"));
    x->x_val = val;
    post("flip~: by r. luke dubois, cmc");
    return (x);
}

void flip_tilde_setup(void)
{
	sigflip_class = class_new(gensym("flip~"), (t_newmethod)sigflip_new, 0,
        sizeof(t_sigflip), 0, A_DEFFLOAT, 0);
    class_addmethod(sigflip_class, nullfn, gensym("signal"), A_NULL);
    class_addmethod(sigflip_class, (t_method)sigflip_dsp, gensym("dsp"), A_NULL);
	class_addfloat(sigflip_class, (t_method)sigflip_float);
    class_sethelpsymbol(sigflip_class, gensym("help-flip~.pd"));
}
#endif /* PD */
