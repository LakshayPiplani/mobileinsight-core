/*
 * LTE NB1 ML1 Cell Resel
 */

#include "consts.h"
#include "log_packet.h"
#include "log_packet_helper.h"

const Fmt LteNb1Ml1CellReselFmt[] = {
    {UINT, "Version", 1},

};

const Fmt LteNb1Ml1CellReselFmt_v4[] = {
    {UINT, "Num Layers", 1},
    {SKIP, NULL, 2}, //Reserved
};

const Fmt LteNb1Ml1CellReselFmt_LayerInfo_v4[] = {
    {UINT, "Num Cells", 2},
    {UINT, "Frequency", 2}, 

};
const Fmt LteNb1Ml1CellReselFmt_CellInfo_v4[] = {
    {UINT, "PCI", 2},
    {UINT, "RANK", 2},
    {UINT, "Tresel Value", 2}, 
    {SKIP, NULL, 2}, //Reserved
};

static int _decode_lte_nb1_ml1_cell_resel_payload (const char *b,
        int offset, size_t length, FieldList &result) {
    int start = offset;
    int pkt_ver = _search_result_int(result, "Version");


    switch (pkt_ver) {
        case 4:
        {
        	offset += _decode_by_fmt(LteNb1Ml1CellReselFmt_v4,
                    ARRAY_SIZE(LteNb1Ml1CellReselFmt_v4, Fmt),
                    b, offset, length, result);
            int num_layers = _search_result_int(result, "Num Layers");

            FieldList result_layers;
            for (int i = 0; i < num_layers; i++) {
                offset += _decode_by_fmt(LteNb1Ml1CellReselFmt_LayerInfo_v4,
                    ARRAY_SIZE(LteNb1Ml1CellReselFmt_LayerInfo_v4, Fmt),
                    b, offset, length, result);
                int num_cells = _search_result_int(result, "Num Cells");

                FieldList result_cells;
                for (int j = 0; j < num_cells; j++) {
                    FieldList result_cells_item;
                    offset += _decode_by_fmt(LteNb1Ml1CellReselFmt_CellInfo_v4,
                            ARRAY_SIZE(LteNb1Ml1CellReselFmt_CellInfo_v4, Fmt),
                            b, offset, length, result_cells_item);

                    int temp = _search_result_int(result_cells_item, "RANK");
                    int iRANK = ((temp & 0xff) << 24) >> 24;
                    _replace_result_int(result_cells_item, "RANK",
                        iRANK);

                    result_cells.push_back({"Ignored", FieldValue{std::move(result_cells_item)}, "dict"});

                }
                result_layers.push_back({"cells", FieldValue{std::move(result_cells)}, "dict"});
            }
            result.push_back({"layers", FieldValue{std::move(result_layers)}, "list"});
            return offset - start;
        }
        default:
            printf("(MI)Unknown LTE NB1 ML1 Cell Resel Info version: 0x%x\n", pkt_ver);
            return 0;
    }
}