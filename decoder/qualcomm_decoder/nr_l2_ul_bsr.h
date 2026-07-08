/*
 * NR5G L2 UL BSR
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt NRL2ULBSR_Fmt [] = {
    {UINT, "Version", 4},
};


//Version2
const Fmt Meta_v2 [] = {
    {UINT, "Num TTI", 4},//4bit
    {PLACEHOLDER, "LCID Prio Bitmask", 0},//16bit
    {PLACEHOLDER, "Reserved1", 0}, //12bit
};
const Fmt TTIInfo_v2 [] = {
    {UINT, "Hard ID", 1},		//4bit  //insert systime
    {PLACEHOLDER, "Carrier", 0},	//4bit
    {UINT, "BSR Trigger Reason", 4}, 	//2bit
    {PLACEHOLDER, "BSR Type", 0},		//4bit		
    {PLACEHOLDER, "BSR Length", 0},		//4bit
    {PLACEHOLDER, "LCG Bitmask", 0},	//8bit
    {PLACEHOLDER, "Num LCIDs", 0},		//4bit
    {PLACEHOLDER, "Reserved3", 0},    	//2bit
    {PLACEHOLDER, "Reserved4", 0},
    {UINT, "Reserved5", 3},   //appendQOS LCID
 		
};
const Fmt SysTime_v2 [] = {
    {UINT, "Slot Number", 1},
    {UINT, "Reserved6", 1},
    //{SKIP, NULL, 1},    		
    {UINT, "FN", 2}, 				//10bit
    {PLACEHOLDER, "Reserved2", 0},  	//6bit
};
const Fmt QOSLCID_v2 [] = {
    {UINT, "QOS LCID", 1},
};
const Fmt Skip1_v2 [] = {
    {UINT, "Reserved11", 4},
};
const Fmt Skip2_v2 [] = {
    {UINT, "Reserved12", 8},
};
const ValueName BSRTriggerReason[] = {
        {1, "T_PERIODIC_EXPIRY_BSR"},
        {4, "HIGH_DATA_ARRIVAL"},
        {5, "T_PERIODIC_EXPIRY_BSR:HIGH_DATA_ARRIVAL"}
};
const ValueName BSRType[] = {
        {1, "SHORT_BSR"},
        {2, "LONG_BSR"}
};
static int _decode_nr_l2_ul_bsr_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");   

    switch (pkt_ver) {
    case 2:
        {

		//Meta
		FieldList result_Meta;			    
		offset += _decode_by_fmt(Meta_v2,
		    ARRAY_SIZE(Meta_v2, Fmt),
		    b, offset, length, result_Meta);
			//划分bit
        unsigned int temp4 = _search_result_uint(result_Meta, "Num TTI");
        int iNumTTI = temp4 & 15; // 4 bits
        int iLCIDPrioBitmask = (temp4 >> 4) & 65535;    // 16 bits	
        int iReserved1 = (temp4 >> 20) & 4095;    // 12 bits
        _replace_result_int(result_Meta,
                "Num TTI", iNumTTI);
        _replace_result_int(result_Meta,
                "LCID Prio Bitmask", iLCIDPrioBitmask);
        _replace_result_int(result_Meta,
                "Reserved1", iReserved1);
		//Meta加入result
		result.push_back({"Meta", FieldValue{std::move(result_Meta)}, "dict"});		
				    
		//TTI Info
		int num_TTI = iNumTTI;
        //unsigned int num_TTI = _search_result_uint(result, "Num TTI");		   
    	//int num_TTI = _search_result_int(result, "Num TTI");		    
        FieldList result_TTI;
        for (int i = 0; i < num_TTI; i++) {
			//SysTime
			FieldList result_SysTime;			    
			offset += _decode_by_fmt(SysTime_v2,
				ARRAY_SIZE(SysTime_v2, Fmt),
				b, offset, length, result_SysTime);
			//SysTime内部划分bit
		    unsigned int temp5 = _search_result_uint(result_SysTime, "FN");
		    int iFN = temp5 & 1023; // 10 bits
		    int iReserved2 = (temp5 >> 10) & 63;    // 6 bits	
		    _replace_result_int(result_SysTime,"FN", iFN);
		    _replace_result_int(result_SysTime,"Reserved2", iReserved2);
		    //TTI Item                    				        
            FieldList result_TTI_item;
            offset += _decode_by_fmt(TTIInfo_v2,
                    ARRAY_SIZE(TTIInfo_v2, Fmt),
                    b, offset, length, result_TTI_item);		    
			//SysTime插入TTIItem
			result_TTI_item.insert(result_TTI_item.begin() + (0), {"SysTime", FieldValue{std::move(result_SysTime)}, "dict"});				    
		    
		    
		    //TTI Info内部划分bit
		    unsigned int temp6 = _search_result_uint(result_TTI_item, "Hard ID");
		    int iHardID = temp6 & 15; // 4 bits
		    int iCarrier = (temp6 >> 4) & 15;    // 4 bits	
		    _replace_result_int(result_TTI_item,"Hard ID", iHardID);
		    _replace_result_int(result_TTI_item,"Carrier", iCarrier);
		    unsigned int temp7 = _search_result_uint(result_TTI_item, "BSR Trigger Reason");
		    int iBSRTriggerReason = temp7 & 7; 	// 3 bits
		    int iBSRType = (temp7 >> 3) & 3;    // 3 bits	
		    int iBSRLength = (temp7 >> 6) & 15; 	// 4 bits
		    int iLCGBitmask = (temp7 >> 10) & 255;// 8 bits	
		    int iNumLCIDs = (temp7 >> 18) & 15; 	// 4 bits
		    int iReserved3 = (temp7 >> 22) & 3;   // 2 bits		
		    int iReserved4 = (temp7 >> 24) & 255;   // 8 bits	    		    
		    _replace_result_int(result_TTI_item,"BSR Trigger Reason", iBSRTriggerReason);
	        (void) _map_result_field_to_name(result_TTI_item,
	                "BSR Trigger Reason",
	                BSRTriggerReason,
	                ARRAY_SIZE(BSRTriggerReason, ValueName),
	                "(MI)Unknown");		    
		    //BSR Trigger Reason和BSR Type要转换字符串
		    _replace_result_int(result_TTI_item,"BSR Type", iBSRType);
	        (void) _map_result_field_to_name(result_TTI_item,
	                "BSR Type",
	                BSRType,
	                ARRAY_SIZE(BSRType, ValueName),
	                "(MI)Unknown");				    	
		    _replace_result_int(result_TTI_item,"BSR Length", iBSRLength);
		    _replace_result_int(result_TTI_item,"LCG Bitmask", iLCGBitmask);
		    _replace_result_int(result_TTI_item,"Num LCIDs", iNumLCIDs);
		    _replace_result_int(result_TTI_item,"Reserved3", iReserved3);
		    _replace_result_int(result_TTI_item,"Reserved4", iReserved4);
		    		    
		    //QOS LCID		    
			int num_LCIDs = _search_result_int(result_TTI_item, "Num LCIDs");		    
		    FieldList result_LCIDs;
		    for (int i = 0; i < num_LCIDs; i++) {
		        FieldList result_LCIDs_item;
		        offset += _decode_by_fmt(QOSLCID_v2,
		                ARRAY_SIZE(QOSLCID_v2, Fmt),
		                b, offset, length, result_LCIDs_item);
		        char name[64];
                int tmp=_search_result_int(result_LCIDs_item,"QOS LCID");
		        sprintf(name, "QOS LCID[%d]", i);
				result_LCIDs.push_back({name, FieldValue{int64_t(tmp)}, ""});
				}	
			//QOS LCID insert TTIItem
			result_TTI_item.push_back({"QOS LCID", FieldValue{std::move(result_LCIDs)}, "dict"});	
			
			//Skip
			FieldList result_Skip;
		    int count = 0;
		    for (int i = 0; i < 8; i++) {
		        if ((iLCGBitmask&(1<<i))==(1<<i)) {
		            count++;
		        }
		    }
		    switch(count){
		    	case 1:	
					offset += _decode_by_fmt(Skip1_v2,
						ARRAY_SIZE(Skip1_v2, Fmt),
						b, offset, length, result_Skip);
					break;
				case 2:	
					offset += _decode_by_fmt(Skip2_v2,
						ARRAY_SIZE(Skip2_v2, Fmt),
						b, offset, length, result_Skip);
					break;
				default:
					offset += _decode_by_fmt(Skip1_v2,
						ARRAY_SIZE(Skip1_v2, Fmt),
						b, offset, length, result_Skip);
				}				
									    					
		
			result_TTI_item.push_back({"Skip", FieldValue{std::move(result_Skip)}, "dict"});

			//TTI Item add TTI
            char name[64];
            sprintf(name, "TTI Info[%d]", i);
			result_TTI.push_back({name, FieldValue{std::move(result_TTI_item)}, "dict"});						    
		    }		    
		//TTI add result
		result.push_back({"TTI Infos", FieldValue{std::move(result_TTI)}, "dict"});	
		return offset - start;	
		}	    
    default:
        printf("(MI)Unknown NR L2 UL BSR Version: 0x%x\n", pkt_ver);
        return 0;
    }
}	    
		    
		    
		    
		    
		    
		    
		    
		    
		    
