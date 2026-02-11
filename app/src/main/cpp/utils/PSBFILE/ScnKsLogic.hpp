#ifndef SCN_KSLOGIC_H
#define SCN_KSLOGIC_H

#include "psb.hpp"
#include "ScnHelpers.h"
#include <fstream>
#include <android/log.h>
#include <unordered_set>
#include <algorithm>

#define TAG_KS "ScnKsLogic"
#define LOGD_KS(...) __android_log_print(ANDROID_LOG_DEBUG, TAG_KS, __VA_ARGS__)

namespace ScnKs {

    inline std::string GetStr(const psb_t& psb, psb_value_t* val) {
        if (!val) return "";
        if (val->get_type() >= psb_value_t::TYPE_STRING_N1 && val->get_type() <= psb_value_t::TYPE_STRING_N4) {
            psb_string_t* strObj = dynamic_cast<psb_string_t*>(val);
            if(strObj) return strObj->get_string();
        }
        return "";
    }

    inline std::string GetNameFromStruct(const psb_t& psb, psb_value_t* val) {
        if (!val) return "";
        if (val->get_type() == psb_value_t::TYPE_COLLECTION) {
            psb_collection_t* col = (psb_collection_t*)val;
            if (col->size() > 0) {
                unsigned char* pObj = col->get(0);
                psb_value_t* vObj = psb.unpack(pObj);
                if (vObj && vObj->get_type() == psb_value_t::TYPE_OBJECTS) {
                    psb_objects_t* obj = (psb_objects_t*)vObj;
                    unsigned char* pName = obj->get_data("name");
                    if (pName) {
                        psb_value_t* vName = psb.unpack(pName);
                        std::string name = GetStr(psb, vName);
                        delete vName; delete vObj;
                        return name;
                    }
                }
                if (vObj) delete vObj;
            }
        }
        return "";
    }

    inline void ProcessList(const psb_t& psb, psb_collection_t* list, std::ofstream& out, int& id) {
        if (!list) return;
        const int LANGUAGE_INDEX = 0;
        for (uint32_t i = 0; i < list->size(); i++) {
            unsigned char* buff = list->get(i);
            psb_value_t* val = psb.unpack(buff);

            if (val && val->get_type() == psb_value_t::TYPE_COLLECTION) {
                psb_collection_t* params = (psb_collection_t*)val;
                std::string nameText = "";
                std::string messageText = "";

                if (params->size() > 1) {
                    unsigned char* p1 = params->get(1);
                    psb_value_t* v1 = psb.unpack(p1);
                    if (v1 && v1->get_type() == psb_value_t::TYPE_COLLECTION) {
                        psb_collection_t* langs = (psb_collection_t*)v1;
                        if (langs->size() > LANGUAGE_INDEX) {
                            unsigned char* lPtr = langs->get(LANGUAGE_INDEX);
                            psb_value_t* lVal = psb.unpack(lPtr);
                            if (lVal && lVal->get_type() == psb_value_t::TYPE_COLLECTION) {
                                psb_collection_t* blk = (psb_collection_t*)lVal;
                                if (blk->size() > 1) {
                                    unsigned char* mPtr = blk->get(1);
                                    psb_value_t* mVal = psb.unpack(mPtr);
                                    messageText = GetStr(psb, mVal);
                                    delete mVal;
                                }
                            }
                            if (lVal) delete lVal;
                        }
                    }
                    delete v1;
                }
                if (params->size() > 2) {
                    unsigned char* p2 = params->get(2);
                    psb_value_t* v2 = psb.unpack(p2);
                    nameText = GetNameFromStruct(psb, v2);
                    delete v2;
                }
                if (!messageText.empty()) {
                    messageText = ScnHelper::Clean(messageText);
                    if (!messageText.empty()) {
                        if (!nameText.empty() && nameText != "＠") {
                            out << id << "|Name|" << ScnHelper::Clean(nameText) << "\n";
                        }
                        out << id << "|Message|" << messageText << "\n";
                        id++;
                    }
                }
            }
            if (val) delete val;
        }
    }

    inline void Traverse(const psb_t& psb, const psb_value_t* node, std::ofstream& out, int& idCounter, int depth = 0) {
        if (!node) return;
        if (depth > 20) return;

        if (node->get_type() == psb_value_t::TYPE_OBJECTS) {

            psb_objects_t* obj = (psb_objects_t*)node;
            for (uint32_t i = 0; i < obj->size(); i++) {
                std::string key = obj->get_name(i);
                unsigned char* ptr = obj->get_data(i);
                psb_value_t* val = psb.unpack(ptr);

                if (key == "texts" || key == "text" || key == "lines") {
                    if (val->get_type() == psb_value_t::TYPE_COLLECTION) {
                        ProcessList(psb, (psb_collection_t*)val, out, idCounter);
                    }
                }

                if (val->get_type() == psb_value_t::TYPE_OBJECTS || val->get_type() == psb_value_t::TYPE_COLLECTION) {
                    Traverse(psb, val, out, idCounter, depth + 1);
                }
                delete val;
            }
        }
        else if (node->get_type() == psb_value_t::TYPE_COLLECTION) {
            psb_collection_t* col = (psb_collection_t*)node;
            for (uint32_t k = 0; k < col->size(); k++) {
                unsigned char* ptr = col->get(k);
                psb_value_t* val = psb.unpack(ptr);
                Traverse(psb, val, out, idCounter, depth + 1);
                delete val;
            }
        }
    }

    inline void Extract(const psb_t& psb, const psb_objects_t* root, std::ofstream& out) {
        int idCounter = 1;
        LOGD_KS("Starting KS Extraction...");
        Traverse(psb, (const psb_value_t*)root, out, idCounter, 0);
        LOGD_KS("Extraction Done. Lines: %d", idCounter);
    }
}

#endif