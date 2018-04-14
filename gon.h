//Glaiel Object Notation
//its json, minus the crap!

#pragma once
#include <string>
#include <map>
#include <vector>

class GonObject {
    public:
        enum gon_type {
            g_string,
            g_number,
            g_object,
            g_array,
            g_bool,
            g_null
        };

        std::map<std::string, int> children_map;
        std::vector<GonObject> children_array;
        int int_data;
        double float_data;
        bool bool_data;
        std::string string_data;

    public:
        static GonObject Load(std::string filename);
        static GonObject LoadFromBuffer(std::string buffer);
        std::string name;

        int type;

        //throw error if accessing wrong type, otherwise return correct type
        std::string String() const;
        int Int() const;
        double Number() const;
        bool Bool() const;

        bool Contains(std::string child) const;
        bool Contains(int child) const;

        const GonObject& operator[](std::string child) const;
        const GonObject& operator[](int childindex) const;
        int Length() const;

        void DebugOut();

        void Save(std::string outfilename);
        std::string getOutStr();
};
