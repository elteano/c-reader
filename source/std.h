#ifndef __STD_H
#define __STD_H

#define TOP_PADDING 3
#define INTER_PADDING 3
#define SIDE_PADDING 3

#define TOP_BORDER_CHAR  "\u2501"
#define TOP_SPLIT_CHAR   "\u2533"
#define SIDE_BORDER_CHAR "\u2503"
#define TL_CORNER_CHAR   "\u250f"
#define TR_CORNER_CHAR   "\u2513"
#define BL_CORNER_CHAR   "\u2517"
#define BR_CORNER_CHAR   "\u251b"
#define CT_CORNER_CHAR   "\u254b"

#define ARROW_CHAR "\u21d2"
#define SILENT_ARROW_CHAR "\u2192"

// Type casts between signed and undeclared (which means signed).
// libxml2 uses the xmlChar type, which is defined as unsigned char.
// This means we get warnings whenever we try to mix what we read from xml with
// standard functions. Appropriate when we call strlen, but sometimes we gotta
// do what we gotta do.
#define __UC(_s) (unsigned char*)(_s)
#define __CC(_s) (char*)(_s)

#endif

