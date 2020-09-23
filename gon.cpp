#include "gon.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>


static bool IsWhitespace(char c){
    return c==' '||c=='\n'||c=='\r'||c=='\t';
}

static bool IsSymbol(char c){
    return c=='='||c==','||c==':'||c=='{'||c=='}'||c=='['||c==']';
}
static bool IsIgnoredSymbol(char c){
    return c=='='||c==','||c==':';
}

static void DefaultGonErrorCallback(const std::string& err){
    throw (std::string)err;
}

std::function<void(const std::string&)> GonObject::ErrorCallback = DefaultGonErrorCallback;
const GonObject GonObject::null_gon;
GonObject GonObject::non_const_null_gon;
std::string GonObject::last_accessed_named_field = "";

static std::vector<std::string> Tokenize(std::string data){
    std::vector<std::string> tokens;

    bool inString = false;
    bool inComment = false;
    bool escaped = false;
    std::string current_token = "";
    for(int i = 0; i<data.size(); i++){
        if(!inString && !inComment){
            if(IsSymbol(data[i])){
                if(current_token != ""){
                    tokens.push_back(current_token);
                    current_token = "";
                }

                if(!IsIgnoredSymbol(data[i])){
                    current_token += data[i];
                    tokens.push_back(current_token);
                    current_token = "";
                }
                continue;
            }

            if(IsWhitespace(data[i])){
                if(current_token != ""){
                    tokens.push_back(current_token);
                    current_token = "";
                }

                continue;
            }

            if(data[i] == '#'){
                if(current_token != ""){
                    tokens.push_back(current_token);
                    current_token = "";
                }

                inComment = true;
                continue;
            }
            if(data[i] == '"'){
                if(current_token != ""){
                    tokens.push_back(current_token);
                    current_token = "";
                }

                inString = true;
                continue;
            }

            current_token += data[i];
        }

        if(inString){
            if(escaped){
                if(data[i] == 'n'){
                    current_token += '\n';
                } else {
                    current_token += data[i];
                }
                escaped = false;
            } else if(data[i] == '\\'){
                escaped = true;
            } else if(!escaped && data[i] == '"'){
                if(current_token != ""){
                    tokens.push_back(current_token);
                    current_token = "";
                }
                inString = false;
                continue;
            } else {
                current_token += data[i];
                continue;
            }
        }

        if(inComment){
            if(data[i] == '\n'){
                inComment = false;
                continue;
            }
        }
    }

    if(current_token != "") tokens.push_back(current_token);

    return tokens;
}

struct GonTokenStream {
    std::vector<std::string> Tokens;
    int current;
    bool error;

    GonTokenStream():current(0),error(false){
    }

    std::string Read(){
        if(current>=Tokens.size()){
            error = true;
            return "!";
        }

        return Tokens[current++];
    }
    std::string Peek(){
        if(current>=Tokens.size()){
            error = true;
            return "!";
        }
        return Tokens[current];
    }

    void Consume(){ //avoid anonymous string creation/destruction
        if(current>=Tokens.size()){
            error = true;
            return;
        }
        current++;
    }
};

GonObject::GonObject(){
    name = "";
    type = FieldType::NULLGON;
    int_data = 0;
    float_data = 0;
    bool_data = 0;
    string_data = "";
}

static GonObject LoadFromTokens(GonTokenStream& Tokens){
    GonObject ret;

    if(Tokens.Peek() == "{"){         //read object
        ret.type = GonObject::FieldType::OBJECT;

        Tokens.Consume(); //consume '{'
        while(Tokens.Peek() != "}"){
            std::string name = Tokens.Read();

            ret.children_array.push_back(LoadFromTokens(Tokens));
            ret.children_map[name] = (int)ret.children_array.size()-1;
            ret.children_array[ret.children_array.size()-1].name = name;

            if(Tokens.error) {
                GonObject::ErrorCallback("GON ERROR: missing a '}' somewhere");
                return GonObject::null_gon;
            }
        }
        Tokens.Consume(); //consume '}'

        return ret;
    } else if(Tokens.Peek() == "["){  //read array
        ret.type = GonObject::FieldType::ARRAY;

        Tokens.Consume(); //consume '['
        while(Tokens.Peek() != "]"){
            ret.children_array.push_back(LoadFromTokens(Tokens));

            if(Tokens.error) {
                GonObject::ErrorCallback("GON ERROR: missing a ']' somewhere");
                return GonObject::null_gon;
            }
        }
        Tokens.Consume(); //consume ']'

        return ret;
    } else {                          //read data value
        ret.type = GonObject::FieldType::STRING;
        ret.string_data = Tokens.Read();

        //if string data can be converted to a number, do so
        char*endptr;
        ret.int_data = strtol(ret.string_data.c_str(), &endptr, 0);
        if(*endptr == 0){
            ret.type = GonObject::FieldType::NUMBER;
        }

        ret.float_data = strtod(ret.string_data.c_str(), &endptr);
        if(*endptr == 0){
            ret.type = GonObject::FieldType::NUMBER;
        }

        //if string data can be converted to a bool or null, convert
        if(ret.string_data == "null") ret.type = GonObject::FieldType::NULLGON;
        if(ret.string_data == "true") {
            ret.type = GonObject::FieldType::BOOL;
            ret.bool_data = true;
        }
        if(ret.string_data == "false") {
            ret.type = GonObject::FieldType::BOOL;
            ret.bool_data = false;
        }

        return ret;
    }
}

GonObject GonObject::Load(const std::string& filename){
    std::ifstream in(filename.c_str(), std::ios::binary);
    in.seekg (0, std::ios::end);
    std::streamoff length = in.tellg();
    in.seekg (0, std::ios::beg);
    std::string str(length + 2, '\0');
    in.read(&str[1], length);
    str.front() = '{';
    str.back() = '}';

    std::vector<std::string> Tokens = Tokenize(str);

    GonTokenStream ts;
    ts.current = 0;
    ts.Tokens = Tokens;

    return LoadFromTokens(ts);
}

GonObject GonObject::LoadFromBuffer(const std::string& buffer){
    std::string str = std::string("{")+buffer+"}";
    std::vector<std::string> Tokens = Tokenize(str);

    GonTokenStream ts;
    ts.current = 0;
    ts.Tokens = Tokens;

    return LoadFromTokens(ts);
}

//options with error throwing
std::string GonObject::String() const {
    if(type == FieldType::NULLGON) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" does not exist");
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" is not a string");
    return string_data;
}
const char* GonObject::CString() const {
    if(type == FieldType::NULLGON) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" does not exist");
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" is not a string");
    return string_data.c_str();
}

int GonObject::Int() const {
    if(type == FieldType::NULLGON) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" does not exist");
    if(type != FieldType::NUMBER) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" is not a number");
    return int_data;
}
double GonObject::Number() const {
    if(type == FieldType::NULLGON) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" does not exist");
    if(type != FieldType::NUMBER) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" is not a number");
    return float_data;
}
bool GonObject::Bool() const {
    if(type == FieldType::NULLGON) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" does not exist");
    if(type != FieldType::BOOL) ErrorCallback("GON ERROR: Field \""+(!name.empty()?name:last_accessed_named_field)+"\" is not a bool");
    return bool_data;
}

//options with a default value
std::string GonObject::String(const std::string& _default) const {
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) return _default;
    return string_data;
}
const char* GonObject::CString(const char* _default) const {
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) return _default;
    return string_data.c_str();
}

int GonObject::Int(int _default) const {
    if(type != FieldType::NUMBER) return _default;
    return int_data;
}
double GonObject::Number(double _default) const {
    if(type != FieldType::NUMBER) return _default;
    return float_data;
}
bool GonObject::Bool(bool _default) const {
    if(type != FieldType::BOOL) return _default;
    return bool_data;
}

bool GonObject::Contains(const std::string& child) const{
    if(type != FieldType::OBJECT) return false;

    auto iter = children_map.find(child);
    if(iter != children_map.end()){
        return true;
    }

    return false;
}
bool GonObject::Contains(int child) const{
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return true;

    if(child < 0) return false;
    if(child >= children_array.size()) return false;
    return true;
}
bool GonObject::Exists() const{
    return type != FieldType::NULLGON;
}

const GonObject& GonObject::ChildOrSelf(const std::string& child) const{
    if(Contains(child)) return (*this)[child];
    return *this;
}
GonObject& GonObject::ChildOrSelf(const std::string& child){
    if(Contains(child)) return (*this)[child];
    return *this;
}

const GonObject& GonObject::operator[](const std::string& child) const {
    last_accessed_named_field = child;

    if(type == FieldType::NULLGON) return null_gon;
    if(type != FieldType::OBJECT) return null_gon;

    auto iter = children_map.find(child);
    if(iter != children_map.end()){
        return children_array[iter->second];
    }

    return null_gon;
}
GonObject& GonObject::operator[](const std::string& child) {
    last_accessed_named_field = child;

    if(type == FieldType::NULLGON) return non_const_null_gon;
    if(type != FieldType::OBJECT) return non_const_null_gon;

    auto iter = children_map.find(child);
    if(iter != children_map.end()){
        return children_array[iter->second];
    }

    return non_const_null_gon;
}
const GonObject& GonObject::operator[](int childindex) const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return *this;
    if(childindex < 0 || childindex >= children_array.size()) return null_gon;
    return children_array[childindex];
}
GonObject& GonObject::operator[](int childindex) {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return *this;
    if(childindex < 0 || childindex >= children_array.size()) return non_const_null_gon;
    return children_array[childindex];
}
int GonObject::Size() const {
    return size();
}


int GonObject::size() const {
    if(type == FieldType::NULLGON) return 0;
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return 1;//size 1, object is self
    return (int)children_array.size();
}
bool GonObject::empty() const {
    return children_array.empty();
}
const GonObject* GonObject::begin() const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this;
    return children_array.data();
}
const GonObject* GonObject::end() const {
    if(type == FieldType::NULLGON) return this;
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this+1;
    return children_array.data()+children_array.size();
}
GonObject* GonObject::begin() {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this;
    return children_array.data();
}
GonObject* GonObject::end() {
    if(type == FieldType::NULLGON) return this;
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this+1;
    return children_array.data()+children_array.size();
}

void GonObject::DebugOut(){
    if(type == FieldType::OBJECT){
        std::cout << name << " is object {" << std::endl;
        for(int i = 0; i<children_array.size(); i++){
            children_array[i].DebugOut();
        }
        std::cout << "}" << std::endl;
    }

    if(type == FieldType::ARRAY){
        std::cout << name << " is array [" << std::endl;
        for(int i = 0; i<children_array.size(); i++){
            children_array[i].DebugOut();
        }
        std::cout << "]" << std::endl;
    }

    if(type == FieldType::STRING){
        std::cout << name << " is string \"" << String() << "\"" << std::endl;
    }

    if(type == FieldType::NUMBER){
        std::cout << name << " is number " << Int() << std::endl;
    }

    if(type == FieldType::BOOL){
        std::cout << name << " is bool " << Bool()<< std::endl;
    }

    if(type == FieldType::NULLGON){
        std::cout << name << " is null " << std::endl;
    }
}

static std::string escaped_string(std::string input){
    auto tokenized = Tokenize(input);
    bool needs_quotes = tokenized.size() != 1 || tokenized[0] != input;

    std::string out_str;
    for(int i = 0; i<input.size(); i++){
        if(input[i] == '\n'){
            needs_quotes = true;
            out_str += "\\n";
        } else if(input[i] == '\\'){
            needs_quotes = true;
            out_str += "\\\\";
        } else if(input[i] == '\"'){
            needs_quotes = true;
            out_str += "\\\"";
        } else {
            out_str += input[i];
        }
    }

    if(needs_quotes) return "\"" + out_str + "\"";
    return out_str;
}

void GonObject::Save(const std::string& filename){
    std::ofstream outfile(filename);
    for(int i = 0; i<children_array.size(); i++){
        outfile << escaped_string(children_array[i].name)+" "+children_array[i].GetOutStr()+"\n";
    }
    outfile.close();
}


std::string GonObject::GetOutStr(const std::string& tab, const std::string& current_tab){
    std::string out = "";

    if(type == FieldType::OBJECT){
        out += "{\n";
        for(int i = 0; i<children_array.size(); i++){
            out += current_tab+tab+escaped_string(children_array[i].name)+" "+children_array[i].GetOutStr(tab, tab+current_tab);
            if(out.back() != '\n') out += "\n";
        }
        out += current_tab+"}\n";
    }

    if(type == FieldType::ARRAY){
        bool short_array = true;
        size_t strlengthtotal = 0;
        for(int i = 0; i<children_array.size(); i++){
            if(children_array[i].type == GonObject::FieldType::ARRAY)
                short_array = false;

            if(children_array[i].type == GonObject::FieldType::OBJECT)
                short_array = false;

            if(children_array[i].type == GonObject::FieldType::STRING)
                strlengthtotal += children_array[i].String().size();


            if(!short_array) break;
        }
        if(strlengthtotal > 80) short_array = false;

        if(short_array){
            out += "[";
            for(int i = 0; i<children_array.size(); i++){
                out += children_array[i].GetOutStr(tab, tab+current_tab);
                if(i != children_array.size()-1) out += " ";
            }
            out += "]\n";
        } else {
            out += "[\n";
            for(int i = 0; i<children_array.size(); i++){
                out += current_tab+tab+children_array[i].GetOutStr(tab, tab+current_tab);
                if(out.back() != '\n') out += "\n";
            }
            out += current_tab+"]\n";
        }
    }

    if(type == FieldType::STRING){
        out += escaped_string(String());
    }

    if(type == FieldType::NUMBER){
        out += std::to_string(Int());
    }

    if(type == FieldType::BOOL){
        if(Bool()){
            out += "true";
        } else {
            out += "false";
        }
    }

    if(type == FieldType::NULLGON){
        out += "null";
    }

    return out;
}


//COMBINING/PATCHING/MERGING STUFF

static bool ends_with(const std::string& str, const std::string& suffix){
    if(str.size() < suffix.size()) return false;
    for(int i = (int)str.size()-1, j = (int)suffix.size()-1; j>=0; j--, i--){
        if(str[i] != suffix[j]) return false;
    }
    return true;
}
static void remove_suffix(std::string& str, const std::string& suffix){
    if(ends_with(str, suffix)){
        str = std::string(str.begin(), str.end() - suffix.size());
    }
}

static GonObject::MergeMode get_patchmode(const std::string& str){
    GonObject::MergeMode policy = GonObject::MergeMode::DEFAULT;
    if(ends_with(str, ".overwrite")){
        policy = GonObject::MergeMode::OVERWRITE;
    } else if(ends_with(str, ".append")){
        policy = GonObject::MergeMode::APPEND;
    } else if(ends_with(str, ".merge")){
        policy = GonObject::MergeMode::MERGE;
    } else if(ends_with(str, ".add")){
        policy = GonObject::MergeMode::ADD;
    } else if(ends_with(str, ".multiply")){
        policy = GonObject::MergeMode::MULTIPLY;
    }
    return policy;
}

static std::string remove_patch_suffixes(const std::string& str){
    std::string res = str;
    remove_suffix(res, ".overwrite");
    remove_suffix(res, ".append");
    remove_suffix(res, ".merge");
    remove_suffix(res, ".add");
    remove_suffix(res, ".multiply");
    return res;
}

static void remove_patch_suffixes_recursive(GonObject& obj){
    remove_suffix(obj.name, ".overwrite");
    remove_suffix(obj.name, ".append");
    remove_suffix(obj.name, ".merge");
    remove_suffix(obj.name, ".add");
    remove_suffix(obj.name, ".multiply");

    if(obj.type == GonObject::FieldType::OBJECT){
        obj.children_map.clear();
        for(int i = 0; i<obj.size(); i++){
            remove_patch_suffixes_recursive(obj.children_array[i]);
            obj.children_map[obj.children_array[i].name] = i;
        }
    } else if(obj.type == GonObject::FieldType::ARRAY){
        for(int i = 0; i<obj.size(); i++){
            remove_patch_suffixes_recursive(obj.children_array[i]);
        }
    }
}

static bool has_patch_suffixes(const std::string& str){
    return get_patchmode(str) != GonObject::MergeMode::DEFAULT;
}


GonObject::MergeMode GonObject::MergePolicyAppend(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::APPEND;
}
GonObject::MergeMode GonObject::MergePolicyMerge(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::MERGE;
}
GonObject::MergeMode GonObject::MergePolicyOverwrite(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::OVERWRITE;
}


void GonObject::InsertChild(const GonObject& other){
    InsertChild(other.name, other);
}
void GonObject::InsertChild(std::string cname, const GonObject& other){
    if(type == FieldType::NULLGON){
        type = FieldType::OBJECT;
    }

    if(type == FieldType::OBJECT){
        children_array.push_back(other);
        children_array.back().name = cname;
        children_map[cname] = (int)children_array.size() - 1;
    } else if(type == FieldType::ARRAY){
        children_array.push_back(other);
        children_array.back().name = "";
    } else {
        ErrorCallback("GON ERROR: Inserting onto incompatible types");
    }
}


void GonObject::Append(const GonObject& other){
    if(type == FieldType::NULLGON){
        *this = other;
    } if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
            children_map[other[i].name] = (int)children_array.size() - 1;
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
        }
    } else if(type == FieldType::STRING && other.type == FieldType::STRING){
        string_data += other.string_data;
    } else {
        ErrorCallback("GON ERROR: Append incompatible types");
    }
}

void GonObject::ShallowMerge(const GonObject& other, std::function<void(const GonObject& a, const GonObject& b)> OnOverwrite){
    if(type == FieldType::NULLGON){
        *this = other;
    } else if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        for(int i = 0; i<other.size(); i++){
            if(Contains(other[i].name)){
                if(OnOverwrite) OnOverwrite((*this)[other[i].name], other[i]);
                children_array[children_map[other[i].name]] = other[i];
            } else {
                children_array.push_back(other[i]);
                children_map[other[i].name] = (int)children_array.size() - 1;
            }
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
        }
    } else {
        ErrorCallback("GON ERROR: Cannot Shallow Merge incompatible types");
    }
}


void GonObject::DeepMerge(const GonObject& other, MergePolicyCallback ObjectMergePolicy, MergePolicyCallback ArrayMergePolicy){
    MergeMode policy = ObjectMergePolicy(*this, other);

    if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        if(policy == MergeMode::APPEND || policy == MergeMode::ADD){
            for(int i = 0; i<other.size(); i++){
                children_array.push_back(other[i]);
                children_map[other[i].name] = (int)children_array.size() - 1;
            }
        } else if(policy == MergeMode::MERGE || policy == MergeMode::DEFAULT || policy == MergeMode::MULTIPLY) {
            for(int i = 0; i<other.size(); i++){
                if(Contains(other[i].name)){
                    children_array[children_map[other[i].name]].DeepMerge(other[i], ObjectMergePolicy, ArrayMergePolicy);
                } else {
                    children_array.push_back(other[i]);
                    children_map[other[i].name] = (int)children_array.size() - 1;
                }
            }
        } else if(policy == MergeMode::OVERWRITE) {
            *this = other;
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        if(policy == MergeMode::APPEND || policy == MergeMode::ADD){
            for(int i = 0; i<other.size(); i++){
                children_array.push_back(other[i]);
            }
        } else if(policy == MergeMode::MERGE || policy == MergeMode::DEFAULT || policy == MergeMode::MULTIPLY) {
            for(int i = 0; i<other.size(); i++){
                if(i < size()){
                    children_array[i].DeepMerge(other[i], ObjectMergePolicy, ArrayMergePolicy);
                } else {
                    children_array.push_back(other[i]);
                }
            }
        } else if(policy == MergeMode::OVERWRITE) {
            *this = other;
        }
    } else if(type == FieldType::STRING && other.type == FieldType::STRING){
        if(policy == MergeMode::ADD){
            string_data += other.string_data;
        } else {
            string_data = other.string_data;
        }
    } else if(type == FieldType::NUMBER && other.type == FieldType::NUMBER){
        if(policy == MergeMode::ADD){
            float_data += other.float_data;
            int_data = int(float_data);
            if(float_data == int_data){
                string_data = std::to_string(int_data);
            } else {
                string_data = std::to_string(float_data);
            }
            bool_data = float_data != 0;
        } else if(policy == MergeMode::MULTIPLY){
            float_data *= other.float_data;
            int_data = int(float_data);
            if(float_data == int_data){
                string_data = std::to_string(int_data);
            } else {
                string_data = std::to_string(float_data);
            }
            bool_data = float_data != 0;
        } else {
            float_data = other.float_data;
            int_data = other.int_data;
            string_data = other.string_data;
            bool_data = other.bool_data;
        }
    } else {
        *this = other;
    }
}


void GonObject::PatchMerge(const GonObject& other){
    MergeMode policy = get_patchmode(other.name);

    if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        if(policy == MergeMode::OVERWRITE) {
            *this = other;
            remove_patch_suffixes_recursive(*this);
        } else {
            for(int i = 0; i<other.size(); i++){
                if(has_patch_suffixes(other[i].name)){
                    std::string other_name = remove_patch_suffixes(other[i].name);
                    if(other_name.empty()){ //patch with self instead of child
                        PatchMerge(other[i]);
                    } else {
                        if(Contains(other_name)){
                            children_array[children_map[other_name]].PatchMerge(other[i]);
                        } else {
                            children_array.push_back(other[i]);
                            remove_patch_suffixes_recursive(children_array.back());
                            children_map[other_name] = (int)children_array.size() - 1;
                        }
                    }
                } else {
                    if(policy == MergeMode::APPEND || policy == MergeMode::ADD){
                        children_array.push_back(other[i]);
                        remove_patch_suffixes_recursive(children_array.back());
                        children_map[other[i].name] = (int)children_array.size() - 1;
                    } else if(policy == MergeMode::MERGE || policy == MergeMode::DEFAULT || policy == MergeMode::MULTIPLY){
                        if(Contains(other[i].name)){
                            children_array[children_map[other[i].name]].PatchMerge(other[i]);
                        } else {
                            children_array.push_back(other[i]);
                            remove_patch_suffixes_recursive(children_array.back());
                            children_map[other[i].name] = (int)children_array.size() - 1;
                        }
                    }
                }
            }
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        if(policy == MergeMode::APPEND || policy == MergeMode::ADD){
            for(int i = 0; i<other.size(); i++){
                children_array.push_back(other[i]);
            }
        } else if(policy == MergeMode::MERGE || policy == MergeMode::DEFAULT || policy == MergeMode::MULTIPLY) {
            for(int i = 0; i<other.size(); i++){
                if(i < size()){
                    children_array[i].PatchMerge(other[i]);
                } else {
                    children_array.push_back(other[i]);
                }
            }
        } else if(policy == MergeMode::OVERWRITE) {
            *this = other;
            remove_patch_suffixes_recursive(*this);
        }
    } else if(type == FieldType::STRING && other.type == FieldType::STRING){
        if(policy == MergeMode::APPEND || policy == MergeMode::ADD){
            string_data += other.string_data;
        } else {
            string_data = other.string_data;
        }
    } else if(type == FieldType::NUMBER && other.type == FieldType::NUMBER){
        if(policy == MergeMode::ADD){
            float_data += other.float_data;
            int_data = int(float_data);
            if(float_data == int_data){
                string_data = std::to_string(int_data);
            } else {
                string_data = std::to_string(float_data);
            }
            bool_data = float_data != 0;
        } else if(policy == MergeMode::MULTIPLY){
            float_data *= other.float_data;
            int_data = int(float_data);
            if(float_data == int_data){
                string_data = std::to_string(int_data);
            } else {
                string_data = std::to_string(float_data);
            }
            bool_data = float_data != 0;
        } else {
            float_data = other.float_data;
            int_data = other.int_data;
            string_data = other.string_data;
            bool_data = other.bool_data;
        }
    } else {
        *this = other;
        remove_patch_suffixes_recursive(*this);
    }
}
