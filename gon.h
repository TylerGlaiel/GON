//Glaiel Object Notation
//its json, minus the crap!

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class GonObject {
    public:
        static const GonObject null_gon;
        static std::string last_accessed_named_field;//used for error reporting when a field is missing, 
                                                     //this assumes you don't cache a field then try to access it later 
                                                     //as the error report for fields uses this value for its message (to avoid creating and destroying a ton of dummy-objects)
                                                     //this isn't a great or super accurate solution for errors, but it's better than nothing

        //default just throws the string, can be set if you want to avoid exceptions
        static std::function<void(const std::string&)> ErrorCallback;

        enum class FieldType {
            NULLGON,
            STRING,
            NUMBER,
            OBJECT,
            ARRAY,
            BOOL
        };

        enum class MergeMode {
            DEFAULT,
            APPEND,
            MERGE,
            OVERWRITE,

            //for numbers:
            ADD,
            MULTIPLY
        };

        std::unordered_map<std::string, int> children_map;
        std::vector<GonObject> children_array;
        int int_data;
        double float_data;
        bool bool_data;
        std::string string_data;
        std::string name;
        FieldType type;

        static MergeMode MergePolicyAppend(const GonObject& field_a, const GonObject& field_b);
        static MergeMode MergePolicyMerge(const GonObject& field_a, const GonObject& field_b);
        static MergeMode MergePolicyOverwrite(const GonObject& field_a, const GonObject& field_b);
        static GonObject Load(const std::string& filename);
        static GonObject LoadFromBuffer(const std::string& buffer);
        typedef std::function<MergeMode(const GonObject& field_a, const GonObject& field_b)> MergePolicyCallback;

        GonObject();

        //throw error if accessing wrong type, otherwise return correct type
        std::string String() const;
        int Int() const;
        double Number() const;
        bool Bool() const;

        //returns a default value if the field doesn't exist or is the wrong type
        std::string String(const std::string& _default) const;
        int Int(int _default) const;
        double Number(double _default) const;
        bool Bool(bool _default) const;

        bool Contains(const std::string& child) const;
        bool Contains(int child) const;
        bool Exists() const; //true if non-null

        //returns null_gon if the field does not exist. 
        const GonObject& operator[](const std::string& child) const;

        //returns self if child does not exist (useful for stuff that can either be a child or the default property of a thing)
        const GonObject& ChildOrSelf(const std::string& child) const;

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
        void Save(const std::string& outfilename);
        std::string GetOutStr(const std::string& tab = "    ", const std::string& current_tab = "");

        //merging/combining functions
        //if self and other are an OBJECT: other will be appended to self
        //if self and other are an ARRAY: other will be appended to self
        //if self and other are STRINGS: the strings are appended
        //if self is a null gon: overwrite self with other
        //otherwise: error
        //(note if a field with the same name is used multiple times, the most recently added one is mapped to the associative array lookup table, however duplicate fields will still exist)
        void Append(const GonObject& other);

        //if self and other are an OBJECT: fields with matching names will be overwritten, new fields appended
        //if self and other are an ARRAY: other will be appended to self
        //if self is a null gon: overwrite self with other
        //otherwise: error
        //(OnOverwrite can be specified if you want a warning or error if gons contain overlapping members)
        //ShallowMerge is not recursive into children, DeepMerge is
        void ShallowMerge(const GonObject& other, std::function<void(const GonObject& a, const GonObject& b)> OnOverwrite = NULL);                                     
                                                  
        //if self and other are an OBJECT: fields with matching names will be DeepMerged, new fields appended
        //if self and other are an ARRAY: fields with matching indexes will be DeepMerged, additional fields appended
        //if self and other mismatch: other will overwrite self
        //ObjectMergePolicy and ArrayMergePolicy can be specified if you want to change how fields merge on a per-field basis
        void DeepMerge(const GonObject& other, MergePolicyCallback ObjectMergePolicy = MergePolicyMerge, MergePolicyCallback ArrayMergePolicy = MergePolicyMerge);


        //similar to deepmerge, however the merge policy for the patch is specified in the patch itself (ex, naming a field "myfield.append" will append it's contents to the end of "myfield" in self
        //possible suffixes: .overwrite, .append, .merge, .add, .multiply
        //with "overwrite", no merge modes specified in sub-fields will have any effect
        //with "append", specifying a merge mode in a sub field will use that mode for that field only
        //"merge" is the default
        //if the patch has node named ".append", ".overwrite", or ".merge", instead of that node getting patched to self, the contents of that node are patched with self instead
        //if both fields are strings: .append will append the strings
        //if both fields are numbers: .add, .multiply can be used to add/subtract numbers
        //.add is treated as .append for non-numerical types, .multiply is treated as .merge for non-numerical types
        void PatchMerge(const GonObject& patch);
};
