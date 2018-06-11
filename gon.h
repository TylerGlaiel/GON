//Glaiel Object Notation
//its json, minus the crap!

#pragma once
#include <string>
#include <map>
#include <vector>

class GonObject {
    public:
        enum gon_type {
            g_null,

            g_string,
            g_number,
            g_object,
            g_array,
            g_bool
        };

        std::map<std::string, int> children_map;
        std::vector<GonObject> children_array;
        int int_data;
        double float_data;
        bool bool_data;
        std::string string_data;

        static GonObject null_gon;

    public:
        static GonObject Load(std::string filename);
        static GonObject LoadFromBuffer(std::string buffer);

        std::string name;
        int type;

        GonObject();

        //throw error if accessing wrong type, otherwise return correct type
        std::string String() const;
        int Int() const;
        double Number() const;
        bool Bool() const;

        //returns a default value if the field doesn't exist or is the wrong type
        std::string String(std::string _default) const;
        int Int(int _default) const;
        double Number(double _default) const;
        bool Bool(bool _default) const;

        bool Contains(std::string child) const;
        bool Contains(int child) const;
        bool Exists() const; //true if non-null

        //returns null_gon if the field does not exist. 
        const GonObject& operator[](std::string child) const;

        //returns self if index is not an array, 
        //all objects can be considered an array of size 1 with themselves as the member
        const GonObject& operator[](int childindex) const;
        int Length() const;

        void DebugOut();

        void Save(std::string outfilename);
        std::string getOutStr();
};
