#ifndef _SCNTXT_H_INCLUDED_
#define _SCNTXT_H_INCLUDED_

#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>
#include <unordered_set>
#include <android/log.h>
#include "psb.hpp"

#define TAG_TXT "ScnTxtLogic"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG_TXT, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG_TXT, __VA_ARGS__)

namespace TXT {

    struct Dialog {
        int id;
        std::string type;
        std::string text;
    };


    inline std::string Clean(std::string text) {
        if (text.empty()) return "";

        try { text = std::regex_replace(text, std::regex("%[^;]+;"), ""); } catch (...) {}

        size_t pos;
        while ((pos = text.find("\\n")) != std::string::npos) text.replace(pos, 2, "");

        try { text = std::regex_replace(text, std::regex("[\r\n]+"), ""); } catch (...) {}

        size_t first = text.find_first_not_of(" \t");
        if (first != std::string::npos) text.erase(0, first);
        size_t last = text.find_last_not_of(" \t");
        if (last != std::string::npos && last + 1 < text.length()) text.erase(last + 1);
        return text;
    }

    inline std::string GetStr(psb_value_t* val) {
        if (!val) return "";
        if (val->get_type() >= psb_value_t::TYPE_STRING_N1 && val->get_type() <= psb_value_t::TYPE_STRING_N4) {
            psb_string_t* strObj = dynamic_cast<psb_string_t*>(val);
            if (strObj) return strObj->get_string();
        }
        return "";
    }

    inline bool IsJunk(std::string str) {
        if (str.empty()) return false;
        static const std::unordered_set<std::string> JUNK = {
                "envupdate", "face", "bg", "ef0", "ef1", "ef2",
                "bgm", "se", "voice", "pretrans", "lock", "unlock", "motion", "pose",
                "select", "jump", "call", "chapter", "trans"
        };
        return JUNK.count(str) > 0;
    }


    inline void ProcessList(psb_t& psb, psb_collection_t* list, std::vector<Dialog>& out, int& id) {
        if (!list) return;
        const int LANGUAGE_INDEX = 0;



        for (uint32_t i = 0; i < list->size(); i++) {
            unsigned char* buff = list->get(i);
            psb_value_t* val = psb.unpack(buff);


            if (val && val->get_type() == psb_value_t::TYPE_COLLECTION) {
                psb_collection_t* params = (psb_collection_t*)val;

                std::string name = "";
                std::string msg = "";
                psb_collection_t* langList = nullptr;
                psb_value_t* vLangContainer = nullptr;

                if (params->size() > 2) {
                    unsigned char* p2 = params->get(2);
                    vLangContainer = psb.unpack(p2);
                    if (vLangContainer && vLangContainer->get_type() == psb_value_t::TYPE_COLLECTION) {

                        langList = (psb_collection_t*)vLangContainer;
                    } else {
                        if (vLangContainer) { delete vLangContainer; vLangContainer = nullptr; }
                    }
                }


                if (!langList && params->size() > 1) {
                    unsigned char* p1 = params->get(1);
                    vLangContainer = psb.unpack(p1);
                    if (vLangContainer && vLangContainer->get_type() == psb_value_t::TYPE_COLLECTION) {

                        langList = (psb_collection_t*)vLangContainer;
                    } else {
                        if (vLangContainer) { delete vLangContainer; vLangContainer = nullptr; }
                    }
                }


                if (langList) {

                    if (params->size() > 0) {
                        unsigned char* p0 = params->get(0);
                        psb_value_t* v0 = psb.unpack(p0);
                        name = GetStr(v0);
                        delete v0;
                    }


                    if (IsJunk(name)) {
                        delete vLangContainer;
                        delete val;
                        continue;
                    }

                    if (langList->size() > LANGUAGE_INDEX) {
                        unsigned char* lPtr = langList->get(LANGUAGE_INDEX);
                        psb_value_t* lVal = psb.unpack(lPtr);

                        if (lVal && lVal->get_type() == psb_value_t::TYPE_COLLECTION) {
                            psb_collection_t* blk = (psb_collection_t*)lVal;

                            if (blk->size() > 1) {
                                unsigned char* mPtr = blk->get(1);
                                psb_value_t* t = psb.unpack(mPtr);
                                msg = GetStr(t);
                                delete t;
                            }
                        }
                        if (lVal) delete lVal;
                    }

                    if (!msg.empty()) {
                        std::string cleanMsg = Clean(msg);
                        if (!cleanMsg.empty()) {
                            if (!name.empty() && name != "＠") {
                                out.push_back({ id, "Name", Clean(name) });
                            }
                            out.push_back({ id++, "Message", cleanMsg });
                        }
                    }
                }


                if (vLangContainer) delete vLangContainer;

            }
            if (val) delete val;
        }
    }

    inline void Extract(psb_t& psb, const psb_objects_t* root, std::vector<Dialog>& out, int& id) {
        if (!root) return;
        struct Local {
            static void Traverse(psb_t& p, psb_value_t* n, std::vector<Dialog>& o, int& i, int depth) {
                if (!n || depth > 10) return;

                if (n->get_type() == psb_value_t::TYPE_OBJECTS) {
                    psb_objects_t* obj = (psb_objects_t*)n;
                    for (uint32_t k = 0; k < obj->size(); k++) {
                        std::string key = obj->get_name(k);
                        unsigned char* ptr = obj->get_data(k);
                        psb_value_t* val = p.unpack(ptr);

                        if (key == "texts" || key == "lines") {
                            if (val && val->get_type() == psb_value_t::TYPE_COLLECTION) {

                                ProcessList(p, (psb_collection_t*)val, o, i);
                            }
                        }
                        else if (val && (val->get_type() == psb_value_t::TYPE_OBJECTS || val->get_type() == psb_value_t::TYPE_COLLECTION)) {
                            Traverse(p, val, o, i, depth + 1);
                        }
                        if (val) delete val;
                    }
                }
                else if (n->get_type() == psb_value_t::TYPE_COLLECTION) {
                    psb_collection_t* col = (psb_collection_t*)n;
                    for (uint32_t k = 0; k < col->size(); k++) {
                        unsigned char* ptr = col->get(k);
                        psb_value_t* val = p.unpack(ptr);
                        Traverse(p, val, o, i, depth + 1);
                        if (val) delete val;
                    }
                }
            }
        };
        Local::Traverse(psb, (psb_value_t*)root, out, id, 0);
    }
}

#endif