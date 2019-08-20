//Glaiel Object Notation
//its json, minus the crap!

#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>

class GonObject {
    public:
        static const GonObject null_gon;
        //default just throws the string, can be set if you want to avoid exceptions
        static std::function<void(std::string)> OnError;

        enum class FieldType {
            NULLGON,

            STRING,
            NUMBER,
            OBJECT,
            ARRAY,
            BOOL
        };

        enum class MergeMode {
            APPEND,
            DEEPMERGE,
            OVERWRITE
        };


        std::map<std::string, int> children_map;
        std::vector<GonObject> children_array;
        int int_data;
        double float_data;
        bool bool_data;
        std::string string_data;

        std::string name;
        FieldType type;

        static MergeMode MergePolicyAppend(const GonObject& field_a, const GonObject& field_b);
        static MergeMode MergePolicyDeepMerge(const GonObject& field_a, const GonObject& field_b);
        static MergeMode MergePolicyOverwrite(const GonObject& field_a, const GonObject& field_b);
        static GonObject Load(std::string filename);
        static GonObject LoadFromBuffer(std::string buffer);
        typedef std::function<MergeMode(const GonObject& field_a, const GonObject& field_b)> MergePolicyCallback;

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

        //returns self if child does not exist (useful for stuff that can either be a child or the default property of a thing)
        const GonObject& ChildOrSelf(std::string child) const;

        //returns self if index is not an array, 
        //all objects can be considered an array of size 1 with themselves as the member, if they are not an ARRAY or an OBJECT
        const GonObject& operator[](int childindex) const;
        int Size() const;

        //compatability with std
        //all objects can be considered an array of size 1 with themselves as the member, if they are not an ARRAY or an OBJECT
        int size() const;
        const GonObject* begin() const;
        const GonObject* end() const;
        GonObject* begin();
        GonObject* end();


        //mostly used for debugging, as GON is not meant for saving files usually
        void DebugOut();
        void Save(std::string outfilename);
        std::string GetOutStr(std::string tab = "    ", std::string current_tab = "");

        //merging/combining functions
        //if self and other are a gon_object: other will be appended to self
        //if self and other are a gon_array: other will be appended to self
        //if self is a null gon, overwrite self with other
        //otherwise: error
        //(note if a field with the same name is used multiple times, the most recently added one is mapped to the associative array lookup table, however duplicate fields will still exist)
        void Append(const GonObject& other);

        //if self and other are a gon_object: fields with matching names will be overwritten, new fields appended
        //if self and other are a gon_array: other will be appended to self
        //if self is a null gon, overwrite self with other
        //otherwise: error
        //(OnOverwrite can be specified if you want a warning or error if gons contain overlapping members)
        void ShallowMerge(const GonObject& other, std::function<void(const GonObject& a, const GonObject& b)> OnOverwrite = NULL);
                                                  
                                                  
        //if self and other are a gon_object: fields with matching names will be DeepMerged, new fields appended
        //if self and other are a gon_array: fields with matching indexes will be DeepMerged, additional fields appended
        //if self and other mismatch: other will overwrite self
        //ObjectMergePolicy and ArrayMergePolicy can be specified if you want to change how fields merge on a per-field basis
        void DeepMerge(const GonObject& other, MergePolicyCallback ObjectMergePolicy = MergePolicyDeepMerge, MergePolicyCallback ArrayMergePolicy = MergePolicyDeepMerge);
};
