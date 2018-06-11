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

        //TODO: escape sequences
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
    type = g_null;
    int_data = 0;
    float_data = 0;
    bool_data = 0;
    string_data = "";
}

GonObject LoadFromTokens(TokenStream& Tokens){
    GonObject ret;

    if(Tokens.Peek() == "{"){         //read object
        ret.type = GonObject::g_object;

        Tokens.Read(); //consume '{'

        while(Tokens.Peek() != "}"){
            std::string name = Tokens.Read();

            ret.children_array.push_back(LoadFromTokens(Tokens));
            ret.children_map[name] = ret.children_array.size()-1;
            ret.children_array[ret.children_array.size()-1].name = name;

            if(Tokens.error) throw "GON ERROR: missing a '}' somewhere";
        }

        Tokens.Read(); //consume '}'

        return ret;
    } else if(Tokens.Peek() == "["){  //read array
        ret.type = GonObject::g_array;

        Tokens.Read(); //consume '['
        while(Tokens.Peek() != "]"){
            ret.children_array.push_back(LoadFromTokens(Tokens));

            if(Tokens.error) throw "GON ERROR: missing a ']' somewhere";
        }
        Tokens.Read(); //consume ']'

        return ret;
    } else {                          //read data value
        ret.type = GonObject::g_string;
        ret.string_data = Tokens.Read();

        //if string data can be converted to a number, do so
        char*endptr;
        ret.int_data = strtol(ret.string_data.c_str(), &endptr, 0);
        if(*endptr == 0){
            ret.type = GonObject::g_number;
        }

        ret.float_data = strtod(ret.string_data.c_str(), &endptr);
        if(*endptr == 0){
            ret.type = GonObject::g_number;
        }

        //if string data can be converted to a bool or null, convert
        if(ret.string_data == "null") ret.type = GonObject::g_null;
        if(ret.string_data == "true") {
            ret.type = GonObject::g_bool;
            ret.bool_data = true;
        }
        if(ret.string_data == "false") {
            ret.type = GonObject::g_bool;
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
    if(type != g_string && type != g_number && type != g_bool) throw "GSON ERROR: Field is not a string";
    return string_data;
}
int GonObject::Int() const {
    if(type != g_number) throw "GON ERROR: Field is not a number";
    return int_data;
}
double GonObject::Number() const {
    if(type != g_number) throw "GON ERROR: Field is not a number";
    return float_data;
}
bool GonObject::Bool() const {
    if(type != g_bool) throw "GON ERROR: Field is not a bool";
    return bool_data;
}

//options with a default value
std::string GonObject::String(std::string _default) const {
    if(type != g_string && type != g_number && type != g_bool) return _default;
    return string_data;
}
int GonObject::Int(int _default) const {
    if(type != g_number) return _default;
    return int_data;
}
double GonObject::Number(double _default) const {
    if(type != g_number) return _default;
    return float_data;
}
bool GonObject::Bool(bool _default) const {
    if(type != g_bool) return _default;
    return bool_data;
}

bool GonObject::Contains(std::string child) const{
    if(type != g_object) return false;

    std::map<std::string, int>::const_iterator iter = children_map.find(child);
    if(iter != children_map.end()){
        return true;
    }

    return false;
}
bool GonObject::Contains(int child) const{
    if(type != g_object && type != g_array) return true;

    if(child < 0) return false;
    if(child >= children_array.size()) return false;
    return true;
}
bool GonObject::Exists() const{
    return type != g_null;
}

const GonObject& GonObject::operator[](std::string child) const {
    if(type == g_null) return null_gon;
    if(type != g_object) throw "GON ERROR: Field is not an object";

    std::map<std::string, int>::const_iterator iter = children_map.find(child);
    if(iter != children_map.end()){
        return children_array[iter->second];
    }

    return null_gon;
    //throw "GON ERROR: Field not found: "+child;
}
const GonObject& GonObject::operator[](int childindex) const {
    if(type != g_object && type != g_array) return *this;//throw "GSON ERROR: Field is not an object or array";
    return children_array[childindex];
}
int GonObject::Length() const {
    if(type != g_object && type != g_array) return 1;//size 1, object is self    //throw "GSON ERROR: Field is not an object or array";
    return children_array.size();
}



void GonObject::DebugOut(){
    if(type == g_object){
        std::cout << name << " is object {" << std::endl;
        for(int i = 0; i<children_array.size(); i++){
            children_array[i].DebugOut();
        }
        std::cout << "}" << std::endl;
    }

    if(type == g_array){
        std::cout << name << " is array [" << std::endl;
        for(int i = 0; i<children_array.size(); i++){
            children_array[i].DebugOut();
        }
        std::cout << "]" << std::endl;
    }

    if(type == g_string){
        std::cout << name << " is string \"" << String() << "\"" << std::endl;
    }

    if(type == g_number){
        std::cout << name << " is number " << Int() << std::endl;
    }

    if(type == g_bool){
        std::cout << name << " is bool " << Bool()<< std::endl;
    }

    if(type == g_null){
        std::cout << name << " is null " << std::endl;
    }
}


void GonObject::Save(std::string filename){
    std::ofstream outfile(filename);
    for(int i = 0; i<children_array.size(); i++){
        outfile << children_array[i].name+" "+children_array[i].getOutStr()+"\n";
    }
    outfile.close();
}

std::string GonObject::getOutStr(){
    std::string out = "";

    if(type == g_object){
        out += "{\n";
        for(int i = 0; i<children_array.size(); i++){
            out += children_array[i].name+" "+children_array[i].getOutStr()+"\n";
        }
        out += "}\n";
    }

    if(type == g_array){
        out += "[";
        for(int i = 0; i<children_array.size(); i++){
            out += children_array[i].getOutStr()+" ";
        }
        out += "]\n";
    }

    if(type == g_string){
        out += String();
    }

    if(type == g_number){
        out += std::to_string(Int());
    }

    if(type == g_bool){
        if(Bool()){
            out += "true";
        } else {
            out += "false";
        }
    }

    if(type == g_null){
        out += "null";
    }

    return out;
}

GonObject GonObject::null_gon;
