/**
 @file
 spinwarrior - a spinwarrior object
 rischko gutleber / susigames.com 
 
 @ingroup	examples	
 */

#include "ext.h"							// standard Max include, always required
#include "ext_obex.h"						// required for new style Max object



////////////////////////// SPINWARRIOR FEATURE
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOReturn.h>
#include <IOKit/hid/IOHIDLib.h>

// IOWARRIOR
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include "SpinKitOsX.h"


CFMutableDictionaryRef CreateDeviceMatchingDict(UInt32 vendorId, UInt32 productId)
{
	CFMutableDictionaryRef result = CFDictionaryCreateMutable(kCFAllocatorDefault,
															  2, 
															  &kCFTypeDictionaryKeyCallBacks, 
															  &kCFTypeDictionaryValueCallBacks);
	
	CFNumberRef vendorIdRef = CFNumberCreate(kCFAllocatorDefault,
											 kCFNumberIntType,
											 &vendorId);
	CFNumberRef productIdRef = CFNumberCreate(kCFAllocatorDefault,
                                              kCFNumberIntType,
                                              &productId);
	
	CFDictionarySetValue(result,
						 CFSTR(kIOHIDVendorIDKey),
						 vendorIdRef);
	
	CFDictionarySetValue(result,
						 CFSTR(kIOHIDProductKey),
						 productIdRef);
	
	CFRelease(vendorIdRef);
	CFRelease(productIdRef);
	
	return result;
}

///
static char SPINKIT_VERSION[] = "SPINWARRIOR NOODLE Version 0.1 (2011-08-13) by Rischkong";
////////////////////////// SPINWARRIOR FEATURE ENDE



////////////////////////// object struct
typedef struct _spinwarrior 
{
	t_object	ob;
	t_atom		val;
	t_symbol	*name;
	void		*out;
    // dump seperate encoders
    void        *outlet_1;
    void        *outlet_2;
    void        *outlet_3;
    //
    t_atom encoder_1[2];
    t_atom encoder_2[2];
    t_atom encoder_3[2];
    
    //
    bool        DEBUG;
    
    // SpinWarrior
    IOHIDDeviceRef spinWarriorDevice;
    IOReturn ioReturnValue ;
    
} t_spinwarrior;

///////////////////////// function prototypes
//// standard set
void *spinwarrior_new(t_symbol *s, long argc, t_atom *argv);
void spinwarrior_free(t_spinwarrior *x);
void spinwarrior_assist(t_spinwarrior *x, void *b, long m, long a, char *s);

void spinwarrior_int(t_spinwarrior *x, long n);
void spinwarrior_float(t_spinwarrior *x, double f);
void spinwarrior_anything(t_spinwarrior *x, t_symbol *s, long ac, t_atom *av);
void spinwarrior_bang(t_spinwarrior *x);
void spinwarrior_identify(t_spinwarrior *x);
void spinwarrior_dblclick(t_spinwarrior *x);
void spinwarrior_acant(t_spinwarrior *x);
///
void spinwarrior_initdevice(t_spinwarrior *x);
void spinwarrior_getdata(t_spinwarrior *x);
void spinwarrior_close(t_spinwarrior *x);



//////////////////////// global class pointer variable
void *spinwarrior_class;


int main(void)
{	
	t_class *c;
	
	c = class_new("spinwarrior", (method)spinwarrior_new, (method)spinwarrior_free, (long)sizeof(t_spinwarrior), 
				  0L /* leave NULL!! */, A_GIMME, 0);
	
    class_addmethod(c, (method)spinwarrior_bang,			"bang", 0);
    class_addmethod(c, (method)spinwarrior_int,			"int",		A_LONG, 0);  
    class_addmethod(c, (method)spinwarrior_float,			"float",	A_FLOAT, 0);  
    class_addmethod(c, (method)spinwarrior_anything,		"anything",	A_GIMME, 0);  
    class_addmethod(c, (method)spinwarrior_identify,		"identify", 0);
	CLASS_METHOD_ATTR_PARSE(c, "identify", "undocumented", gensym("long"), 0, "1");
    
	// we want to 'reveal' the otherwise hidden 'xyzzy' method
    class_addmethod(c, (method)spinwarrior_anything,		"xyzzy", A_GIMME, 0);
    class_addmethod(c, (method)spinwarrior_anything,		"info", A_GIMME, 0);
    class_addmethod(c, (method)spinwarrior_anything,		"init", A_GIMME, 0);
    class_addmethod(c, (method)spinwarrior_anything,		"close", A_GIMME, 0);
    
    // DumpAll
    class_addmethod(c, (method)spinwarrior_anything,		"dump", A_GIMME, 0);
    
    class_addmethod(c, (method)spinwarrior_anything,		"debug", A_GIMME, 0);
    
	// here's an otherwise undocumented method, which does something that the user can't actually 
	// do from the patcher however, we want them to know about it for some weird documentation reason. 
	// so let's make it documentable. it won't appear in the quickref, because we can't send it from a message.
	class_addmethod(c, (method)spinwarrior_acant,			"blooop", A_CANT, 0);	
	CLASS_METHOD_ATTR_PARSE(c, "blooop", "documentable", gensym("long"), 0, "1");
    
	/* you CAN'T call this from the patcher */
    class_addmethod(c, (method)spinwarrior_assist,			"assist",		A_CANT, 0);  
    class_addmethod(c, (method)spinwarrior_dblclick,		"dblclick",		A_CANT, 0);
	
	CLASS_ATTR_SYM(c, "name", 0, t_spinwarrior, name);
	
	class_register(CLASS_BOX, c);
	spinwarrior_class = c;
    
        
	return 0;
}

void spinwarrior_initdevice(t_spinwarrior *x)
{
    object_post((t_object *)x, "Starting Init Routine....");
    // Create HID manager ref
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    
    // crate matching dict
    CFDictionaryRef matchingDict = CreateDeviceMatchingDict(SPINKIT_VID, SPINKIT_PID24A3);
    
    // set matching dict to manager
    IOHIDManagerSetDeviceMatching(manager, matchingDict);
    CFRelease(matchingDict);
    
    // open it
    IOReturn ioResult = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    assert(kIOReturnSuccess == ioResult);
    
    // get set of matching devices
    CFSetRef matchingSet = IOHIDManagerCopyDevices(manager);
    if (!matchingSet) {
        object_post((t_object *)x, "NO SpinWarrior attached... ByeBye!");
        // closing HIDManager;
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        x->spinWarriorDevice = NULL;
        //  return 0;
    } else {
        object_post((t_object *)x, "Found SpinWarrior!");
        // get the devices
        CFIndex setCount = CFSetGetCount(matchingSet);
        CFTypeRef *devices = malloc(setCount * sizeof(CFTypeRef));
        CFSetGetValues(matchingSet, devices); // not retained
        // query the first for its type
        x->spinWarriorDevice = (IOHIDDeviceRef)CFRetain(devices[0]);
    }
    
}

void spinwarrior_getdata(t_spinwarrior *x)
{
    // object_post((t_object *)x, "dump");
    // dumpout data as binary??? 
    if (x->spinWarriorDevice) 
    {
        uint8_t *report = malloc(64); // a little extra
        CFIndex reportSize = 8;
        // IOReturn ioReturnValue = kIOReturnSuccess;
        x->ioReturnValue = kIOReturnSuccess;
        
        // open the device for exclusive access
         x->ioReturnValue = IOHIDDeviceOpen(x->spinWarriorDevice, kIOHIDOptionsTypeSeizeDevice);
        if ( x->ioReturnValue == kIOReturnSuccess) {
            // object_post((t_object *)x, "device opened\n");
        }
        else
        {
            object_post((t_object *)x, "error opening: %d\n",  x->ioReturnValue);
        }
        
        //  while(!done){
        // poll the button push count from the device IOHIDDeviceGetReport
         x->ioReturnValue = IOHIDDeviceGetReport(x->spinWarriorDevice, 0, 0x0300, report, &reportSize);
        
        if ( x->ioReturnValue == kIOReturnSuccess) {
            
            //  object_post((t_object *)x, "DATA size %d >> %d %d | %d %d | %d %d", (int)reportSize, report[0], report[1], report[2], report[3], report[4], report[5]);
        
            // and now all Encoders to seperate outlets 2(X) 3(Y) 4(Z)
            // DEBUG POSITION
            if(x->DEBUG){
                object_post((t_object *)x, "ENCODER 1: %d %d", report[0], report[1]);        
                object_post((t_object *)x, "ENCODER 2: %d %d", report[2], report[3]);        
                object_post((t_object *)x, "ENCODER 3: %d %d", report[4], report[5]);        
            }
            
            // ENCODER 3
            long ac3;
            ac3 = 2;
            t_atom av3[2];
            SETLONG(av3+0, report[4]);
            SETLONG(av3+1, report[5]);
            outlet_anything(x->outlet_1,atom_getsym(av3), ac3, av3); 
            
            // ENCODER 2
            long ac2;
            ac2 = 2;
            t_atom av2[2];
            SETLONG(av2+0, report[2]);
            SETLONG(av2+1, report[3]);
            outlet_anything(x->outlet_2,atom_getsym(av2), ac2, av2);     
            
            // ENCODER 1
            long ac;
            ac = 2;
            t_atom av[2];
            SETLONG(av+0, report[0]);
            SETLONG(av+1, report[1]);
            outlet_anything(x->outlet_3,atom_getsym(av), ac, av);
             
            // ALL ENCODERS
            long acAll;
            acAll = 6;
            t_atom avAll[6];
            SETLONG(avAll+0, report[0]);
            SETLONG(avAll+1, report[1]);
            SETLONG(avAll+2, report[2]);
            SETLONG(avAll+3, report[3]);
            SETLONG(avAll+4, report[4]);
            SETLONG(avAll+5, report[5]);
            outlet_anything(x->out,atom_getsym(avAll), acAll, avAll);
            
            
        }
        else
        {
            object_post((t_object *)x, "*** ERROR getting report: %d\n", x->ioReturnValue);
        }
    }
}

void spinwarrior_close(t_spinwarrior *x)
{
    //
    if(x->spinWarriorDevice){
        // close the device
        x->ioReturnValue = IOHIDDeviceClose(x->spinWarriorDevice, kIOHIDOptionsTypeNone);
      
        if (x->ioReturnValue == kIOReturnSuccess) {
            object_post((t_object *)x, "Device closed! Thank You!");
        }
        else
        {
            object_post((t_object *)x, "*** ERROR closing: %d", x->ioReturnValue);
        }
        x->spinWarriorDevice = NULL;
    }
}


////////////////////////////////////////////////////////// 
void spinwarrior_acant(t_spinwarrior *x)
{
	object_post((t_object *)x, "can't touch this!");
}

void spinwarrior_assist(t_spinwarrior *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	} 
	else {	// outlet
        switch(a)
        {
            case 3:
                sprintf(s, "Dump of All Encoders ");   	
                break;
            case 1:
                sprintf(s, "Encoder Y %ld", a);   	
                break;
            case 2:
                sprintf(s, "Encoder Z %ld", a);   	
                break;
            case 0:
                sprintf(s, "Encoder X %ld", a);   	
                break;
        }
	}
}

void spinwarrior_free(t_spinwarrior *x)
{
	;
}

void spinwarrior_dblclick(t_spinwarrior *x)
{
	object_post((t_object *)x, "UAHAHAHAHA");
}

void spinwarrior_int(t_spinwarrior *x, long n)
{
	atom_setlong(&x->val, n);
	spinwarrior_bang(x);
}

void spinwarrior_float(t_spinwarrior *x, double f)
{
	atom_setfloat(&x->val, f);
	spinwarrior_bang(x);
}

void spinwarrior_anything(t_spinwarrior *x, t_symbol *s, long ac, t_atom *av)
{
	if (s == gensym("xyzzy")) {
		object_post((t_object *)x, "A hollow voice says 'Plugh'");
        
	} else if (s== gensym("info")){
        object_post((t_object *)x, "%s", SPINKIT_VERSION);
        
    } else if (s== gensym("init")){
        spinwarrior_initdevice(x);
        
    } else if (s== gensym("dump")){
        spinwarrior_getdata(x);
   
    } else if (s== gensym("close")){
        spinwarrior_close(x);

    } else if (s== gensym("debug")){
        if(x->DEBUG){x->DEBUG=FALSE;}else{x->DEBUG=TRUE;}
        object_post((t_object *)x, "DEBUG: %d", x->DEBUG);        
        
    } else {		
        atom_setsym(&x->val, s);
		spinwarrior_bang(x);
	}
}

void spinwarrior_bang(t_spinwarrior *x)
{
	switch (x->val.a_type) {
		case A_LONG: outlet_int(x->out, atom_getlong(&x->val)); break;
		case A_FLOAT: outlet_float(x->out, atom_getfloat(&x->val)); break;
		case A_SYM: outlet_anything(x->out, atom_getsym(&x->val), 0, NULL); break;
		default: break;
	}
}

void spinwarrior_identify(t_spinwarrior *x)
{
	object_post((t_object *)x, "My Name Is %s", x->name->s_name);
}

void *spinwarrior_new(t_symbol *s, long argc, t_atom *argv)
{
	t_spinwarrior *x = NULL;
    
	if ((x = (t_spinwarrior *)object_alloc(spinwarrior_class))) {
		x->name = gensym("");
		if (argc && argv) {
			x->name = atom_getsym(argv);
		}
		if (!x->name || x->name == gensym(""))
			x->name = symbol_unique();
		
		atom_setlong(&x->val, 0);
		x->out = outlet_new(x, NULL);
        // seperates
        x->outlet_1 = outlet_new(x, NULL);
        x->outlet_2 = outlet_new(x, NULL);
        x->outlet_3 = outlet_new(x, NULL);
        
	}
    object_post((t_object *)x, "%s", SPINKIT_VERSION);
    x->DEBUG = FALSE;
	return (x);
}
