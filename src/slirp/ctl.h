#define CTL_CMD		0
#define CTL_GATEWAY	1
#define CTL_ALIAS	2
#define CTL_DNS		3
#define CTL_NFSD    254

#define CTL_NET          0x0A000000 //10.0.0.0
#define CTL_NET_MASK     0xFF000000 //255.0.0.0
#define CTL_BASE  	     (CTL_NET | 0x00000200) //10.0.2.0
