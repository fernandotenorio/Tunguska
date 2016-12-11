#ifndef PV_ENTRY_H
#define PV_ENTRY_H

#include "defs.h"

class PVEntry{
	public:
		U64 zKey;
		int move;

		PVEntry(){zKey = 0; move = 0;}

		PVEntry(U64 key, int mv){
			zKey = key;
			move = mv;
		}
};

#endif