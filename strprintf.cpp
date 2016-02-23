#include "strprintf.h"

bool isxdigit(char c) {
	if (c < 0x30) return false;
	if (c > 0x39) {
		c &= ~0x20;
		if (c < 0x41 || c > 0x46) return false;
	}
	return true;
}

unsigned char atox(char c) {
	if (c < 0x3A) return c - 0x30;
	c &= ~0x20;
	c -= (0x41 - 0x0A);
	return c;
}

static const unsigned long dv[] = {
//  4294967296      // 32 bit unsigned max
    1000000000,     // +0
     100000000,     // +1
      10000000,     // +2
       1000000,     // +3
        100000,     // +4
//       65535      // 16 bit unsigned max
         10000,     // +5
          1000,     // +6
           100,     // +7
            10,     // +8
             1,     // +9
};
unsigned int strqtoa(char *p, unsigned int idx, unsigned int mask,
				     unsigned long x, const unsigned long *dp,
					 unsigned char q) {
	char c;
	unsigned long d;
	const unsigned long *qp;
	qp = dv+(sizeof(dv)/sizeof(long)-1);
	qp -= q;

	if (x) {
		while(x < *dp && dp != qp) ++dp;
		do {
			d = *dp;
			c = '0';
			if (x < d) {
				p[idx++] = c;
				idx &= mask;
			} else {
				while (x >= d) ++c, x -= d;
				p[idx++] = c;
				idx &= mask;
			}
			if (dp == qp && q) {
				p[idx++] = '.';
				idx &= mask;
			}
			dp++;
		} while (!(d & 1));
	} else {
		p[idx++] = '0';
		idx &= mask;
		p[idx++] = '.';
		idx &= mask;
		dp += q;
		while (!(*dp++ & 0x1)) {
			p[idx++] = '0';
			idx &= mask;
		}
	}
	return idx;
}

unsigned int strxtoa(char *p, unsigned int idx, unsigned int mask,
					 unsigned long x, const unsigned long *dp) {
    char c;
    unsigned long d;
    if(x) {
        while(x < *dp) ++dp;
        do {
            d = *dp++;
            c = '0';
            while(x >= d) ++c, x -= d;
            p[idx++] = c;
            idx &= mask;
        } while(!(d & 1));
    } else {
    	p[idx++] = '0';
    	idx &= mask;
    }
    return idx;
}

void strputh(char *p, unsigned n) {
    static const char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    *p++ = hex[n % 16];
}

char *strprintf(char *p, const char *format, ...) {
	unsigned char idx;
	va_list a;
	va_start(a, format);
	idx=vstrprintf(p, 0, 0xFFFF, format, a);
	va_end(a);
	return p+idx;
}

unsigned int vstrprintf(char *p, unsigned int idx, unsigned int mask, const char *format, va_list a)
{
    char c;
    int i;
    long n;

    while(c = *format++) {
        if(c == '%') {
            switch(c = *format++) {
                case 's':                       // String
                    idx=strputs(p,idx,mask,va_arg(a, char*));
                    break;
                case 'c':                       // Char
                    p[idx++] = va_arg(a, char);
                    idx &= mask;
                    break;
                case 'i':                       // 16 bit Integer
                case 'u':                       // 16 bit Unsigned
                    i = va_arg(a, int);
                    if(c == 'i' && i < 0) {
                        i = -i;
                        p[idx++] = '-';
                        idx &= mask;
                    }
                    idx=strxtoa(p,idx, mask, (unsigned)i, dv + 5);
                    break;
                case 'l':                       // 32 bit Long
                case 'n':                       // 32 bit uNsigned loNg
                    n = va_arg(a, long);
                    if(c == 'l' &&  n < 0) {
                        n = -n;
                        p[idx++] = '-';
                        idx &= mask;
                    }
                    idx=strxtoa(p, idx,mask,(unsigned long)n, dv);
                    break;
                case 'q':
                case 'Q':
                	// Fractional printing using powers of 10. %q2 means divide by 100.
                	// %Q is unsigned.
                	// %q3 for instance ranges from -32.768 to 32.767.
                	i = va_arg(a, int);
            		if (c == 'q' && i < 0) {
            			i = -i;
            			p[idx++] = '-';
            			idx &= mask;
            		}
                	if (*format < 0x30 || *format > 0x39) {
                		idx = strqtoa(p, idx, mask,(unsigned) i, dv+5, 2);
                	} else {
                		idx = strqtoa(p, idx, mask,(unsigned) i, dv+5, *format - 0x30);
                		format++;
                	}
                	break;
                case 'X':						// 8 bit heXadecimal
                	c = va_arg(a, char);
                	strputh(p+idx, c>>4);
                	idx++;
                	idx &= mask;
                	strputh(p+idx, c);
                	idx++;
                	idx &= mask;
                	break;
                case 'x':                       // 16 bit heXadecimal
                    i = va_arg(a, int);
                    strputh(p+idx,i >> 12);
                    idx++;
                    idx &= mask;
                    strputh(p+idx,i >> 8);
                    idx++;
                    idx &= mask;
                    strputh(p+idx,i >> 4);
                    idx++;
                    idx &= mask;
                    strputh(p+idx,i);
                    idx++;
                    idx &= mask;
                    break;
                case 0: return idx;
                default:
                	p[idx++] = c;
                	idx &= mask;
                	break;
            }
        } else {
        	p[idx++] = c;
        	idx &= mask;
        }
    }
    return idx;
}
