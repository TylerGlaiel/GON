#include "gon.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

bool IsWhitespace(char c){
    return c==' '||c=='\n'||c=='\r'||c=='\t';
}

bool IsSymbol(char c){
    return c=='='||c==','||c==':'||c=='{'||c=='}'||c=='['||c==']';
}
bool IsIgnoredSymbol(char c){
    return c=='='||c==','||c==':';
}

void ThrowString(std::string err){
    throw err;
}

std::function<void(std::string)> GonObject::OnError = ThrowString;
const GonObject GonObject::null_gon;

GonObject::MergeMode GonObject::MergePolicyAppend(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::APPEND;
}
GonObject::MergeMode GonObject::MergePolicyDeepMerge(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::DEEPMERGE;
}
GonObject::MergeMode GonObject::MergePolicyOverwrite(const GonObject& field_a, const GonObject& field_b){
    return MergeMode::OVERWRITE;
}

std::vector<std::string> Tokenize(std::string data){
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

    return tokens;
}

struct TokenStream {
    std::vector<std::string> Tokens;
    int current;
    bool error;

    TokenStream():current(0),error(false){
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
};

GonObject::GonObject(){
    name = "";
    type = FieldType::NULLGON;
    int_data = 0;
    float_data = 0;
    bool_data = 0;
    string_data = "";
}

GonObject LoadFromTokens(TokenStream& Tokens){
    GonObject ret;

    if(Tokens.Peek() == "{"){         //read object
        ret.type = GonObject::FieldType::OBJECT;

        Tokens.Read(); //consume '{'

        while(Tokens.Peek() != "}"){
            std::string name = Tokens.Read();

            ret.children_array.push_back(LoadFromTokens(Tokens));
            ret.children_map[name] = ret.children_array.size()-1;
            ret.children_array[ret.children_array.size()-1].name = name;

            if(Tokens.error) {
                GonObject::OnError("GON ERROR: missing a '}' somewhere");
                return GonObject::null_gon;
            }
        }

        Tokens.Read(); //consume '}'

        return ret;
    } else if(Tokens.Peek() == "["){  //read array
        ret.type = GonObject::FieldType::ARRAY;

        Tokens.Read(); //consume '['
        while(Tokens.Peek() != "]"){
            ret.children_array.push_back(LoadFromTokens(Tokens));

            if(Tokens.error) {
                GonObject::OnError("GON ERROR: missing a ']' somewhere");
                return GonObject::null_gon;
            }
        }
        Tokens.Read(); //consume ']'

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

    return ret;
}



GonObject GonObject::Load(std::string filename){
    std::ifstream in(filename.c_str(), std::ios::binary);
    in.seekg (0, std::ios::end);
    int length = in.tellg();
    in.seekg (0, std::ios::beg);
    std::string str(length + 2, '\0');
    in.read(&str[1], length);
    str.front() = '{';
    str.back() = '}';

    std::vector<std::string> Tokens = Tokenize(str);

    TokenStream ts;
    ts.current = 0;
    ts.Tokens = Tokens;

    return LoadFromTokens(ts);
}

GonObject GonObject::LoadFromBuffer(std::string buffer){
    std::string str = std::string("{")+buffer+"}";
    std::vector<std::string> Tokens = Tokenize(str);

    TokenStream ts;
    ts.current = 0;
    ts.Tokens = Tokens;

    return LoadFromTokens(ts);
}

//options with error throwing
std::string GonObject::String() const {
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) OnError("GON ERROR: Field is not a string");
    return string_data;
}
int GonObject::Int() const {
    if(type != FieldType::NUMBER) OnError("GON ERROR: Field is not a number");
    return int_data;
}
double GonObject::Number() const {
    if(type != FieldType::NUMBER) OnError("GON ERROR: Field is not a number");
    return float_data;
}
bool GonObject::Bool() const {
    if(type != FieldType::BOOL) OnError("GON ERROR: Field is not a bool");
    return bool_data;
}

//options with a default value
std::string GonObject::String(std::string _default) const {
    if(type != FieldType::STRING && type != FieldType::NUMBER && type != FieldType::BOOL) return _default;
    return string_data;
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

bool GonObject::Contains(std::string child) const{
    if(type != FieldType::OBJECT) return false;

    std::map<std::string, int>::const_iterator iter = children_map.find(child);
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

const GonObject& GonObject::ChildOrSelf(std::string child) const{
    if(Contains(child)) return (*this)[child];
    return *this;
}

const GonObject& GonObject::operator[](std::string child) const {
    if(type == FieldType::NULLGON) return null_gon;
    if(type != FieldType::OBJECT) return null_gon;

    std::map<std::string, int>::const_iterator iter = children_map.find(child);
    if(iter != children_map.end()){
        return children_array[iter->second];
    }

    return null_gon;
}
const GonObject& GonObject::operator[](int childindex) const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return *this;
    return children_array[childindex];
}
int GonObject::Size() const {
    return size();
}


int GonObject::size() const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return 1;//size 1, object is self
    return children_array.size();
}
const GonObject* GonObject::begin() const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this;
    return children_array.data();
}
const GonObject* GonObject::end() const {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this+1;
    return children_array.data()+children_array.size();
}
GonObject* GonObject::begin() {
    if(type != FieldType::OBJECT && type != FieldType::ARRAY) return this;
    return children_array.data();
}
GonObject* GonObject::end() {
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


void GonObject::Save(std::string filename){
    std::ofstream outfile(filename);
    for(int i = 0; i<children_array.size(); i++){
        outfile << children_array[i].name+" "+children_array[i].GetOutStr()+"\n";
    }
    outfile.close();
}

std::string GonObject::GetOutStr(std::string tab, std::string current_tab){
    std::string out = "";

    if(type == FieldType::OBJECT){
        out += "{\n";
        for(int i = 0; i<children_array.size(); i++){
            out += current_tab+tab+children_array[i].name+" "+children_array[i].GetOutStr(tab, tab+current_tab);
            if(out.back() != '\n') out += "\n";
        }
        out += current_tab+"}\n";
    }

    if(type == FieldType::ARRAY){
        bool short_array = true;
        for(int i = 0; i<children_array.size(); i++){
            if(children_array[i].type == GonObject::FieldType::ARRAY)
                short_array = false;

            if(children_array[i].type == GonObject::FieldType::OBJECT)
                short_array = false;

            if(children_array[i].type == GonObject::FieldType::STRING && children_array[i].String().size() > 10)
                short_array = false;

            if(!short_array) break;
        }

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
        out += String();
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


void GonObject::Append(const GonObject& other){
    if(type == FieldType::NULLGON){
        *this = other;
    } if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
            children_map[other[i].name] = children_array.size() - 1;
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
        }
    } else {
        OnError("GON ERROR: Cannot Shallow Merge incompatible types");
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
                children_map[other[i].name] = children_array.size() - 1;
            }
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        for(int i = 0; i<other.size(); i++){
            children_array.push_back(other[i]);
        }
    } else {
        OnError("GON ERROR: Cannot Shallow Merge incompatible types");
    }
}


void GonObject::DeepMerge(const GonObject& other, MergePolicyCallback ObjectMergePolicy, MergePolicyCallback ArrayMergePolicy){
    if(type == FieldType::OBJECT && other.type == FieldType::OBJECT){
        MergeMode policy = ObjectMergePolicy(*this, other);

        if(policy == MergeMode::APPEND){
            for(int i = 0; i<other.size(); i++){
                children_array.push_back(other[i]);
                children_map[other[i].name] = children_array.size() - 1;
            }
        } else if(policy == MergeMode::DEEPMERGE) {
            for(int i = 0; i<other.size(); i++){
                if(Contains(other[i].name)){
                    children_array[children_map[other[i].name]].DeepMerge(other[i]);
                } else {
                    children_array.push_back(other[i]);
                    children_map[other[i].name] = children_array.size() - 1;
                }
            }
        } else if(policy == MergeMode::OVERWRITE) {
            *this = other;
        }
    } else if(type == FieldType::ARRAY && other.type == FieldType::ARRAY){
        MergeMode policy = ArrayMergePolicy(*this, other);

        if(policy == MergeMode::APPEND){
            for(int i = 0; i<other.size(); i++){
                children_array.push_back(other[i]);
            }
        } else if(policy == MergeMode::DEEPMERGE) {
            for(int i = 0; i<other.size(); i++){
                if(i < size()){
                    children_array[i].DeepMerge(other[i]);
                } else {
                    children_array.push_back(other[i]);
                }
            }
        } else if(policy == MergeMode::OVERWRITE) {
            *this = other;
        }
    } else {
        *this = other;
    }
}
