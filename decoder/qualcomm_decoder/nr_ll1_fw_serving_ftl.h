/*
 * NR5G LL1 FW Serving FTL
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"
#include<cmath>
#include<string>
const Fmt NrLl1FwServingFtl_Fmt [] = {
    {UINT, "Version", 4},
};

//Version7
const Fmt NrLl1FwServingFtl_v7 [] = {
    {SKIP, NULL, 1},
    {UINT, "Cell Id", 2},//high 10b
    {UINT, "SSB Or TRS", 1},//string
    {UINT, "Carrier Index", 1}, 
    {SKIP, NULL, 2},
    {UINT, "HST Mode", 1},         
    {SKIP, NULL, 64},      
};
const Fmt FtlSnr_v7 [] = {
    {UINT, "FTL SNR Raw", 4},
    {UINT, "FTL SNR", 0},//calculate
};
const ValueName SSBOrTRS[] = {
        {0, "SSB"},
        {1, "TRS"},
};
static int _decode_nr_ll1_fw_serving_ftl_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    //int start = offset;
    int pkt_ver = _search_result_int(result, "Version");
    switch (pkt_ver) {
    case 7:{
    	//Version7
		offset += _decode_by_fmt(NrLl1FwServingFtl_v7,
		    ARRAY_SIZE(NrLl1FwServingFtl_v7, Fmt),
		    b, offset, length, result); 
        unsigned int temp1 = _search_result_uint(result, "Cell Id");
        int iCellId = (temp1 >> 6) & 1023;    //high 10 bits	
        _replace_result_int(result,
                "Cell Id", iCellId);
        int tmp=_search_result_int(result,"SSB Or TRS");
        int value=tmp&0b1;
        _replace_result_int(result,
                  "SSB Or TRS",value );

		(void) _map_result_field_to_name(result,
			    "SSB Or TRS",
			    SSBOrTRS,
			    ARRAY_SIZE(SSBOrTRS, ValueName),
			    "(MI)Unknown");		
		
		//FtlSnr 
		int num_FtlSnr = 4;
		FieldList result_FtlSnr;
		for (int i = 0; i < num_FtlSnr; i++) {
			FieldList result_FtlSnr_item;
			offset += _decode_by_fmt(FtlSnr_v7,
				ARRAY_SIZE(FtlSnr_v7, Fmt),
				b, offset, length, result_FtlSnr_item);
            unsigned int iFTLSNRRaw = _search_result_uint(result_FtlSnr_item,
                    "FTL SNR Raw");
            if (iFTLSNRRaw==0){
                std:: string na="NA";
                _replace_result_string(result_FtlSnr_item,
                      "FTL SNR", na);
            }
            else{
            //SNR=4.241*ln(RAW-30.2)-29.27
            float iFTLSNR = 4.241*(log(iFTLSNRRaw-30.2))-29.27;
            //int iFTLSNR2 = floor(iFTLSNR*100+0.5);
            //float iFTLSNR3 = iFTLSNR2/100.00;
            char snr[64];
            sprintf(snr,"%.2f",iFTLSNR);
            _replace_result(result_FtlSnr_item,
                    "FTL SNR", FieldValue{std::string(snr)});
            }
		    
		    
		    //FtlSnr_item > FtlSnr
		    char name[64];
		    sprintf(name, "FtlSnr[%d]", i);
			result_FtlSnr.push_back({name, FieldValue{std::move(result_FtlSnr_item)}, "dict"});		            
		    }	
		    
		//FtlSnr > result        		
		result.push_back({"FtlSnr", FieldValue{std::move(result_FtlSnr)}, "list"});
        break;
    }	
    default:
        printf("(MI)Unknown NR5G LL1 FW Serving FTL version: %d\n", pkt_ver);
    }
    return 0;
}				
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
