#include "binding_generator.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <functional>
#if __has_include("binding_type_catalog.gen.h")
#include "binding_type_catalog.gen.h"
#else
namespace meowody { struct BindingTypeInfo { std::string cpp,header; }; inline const std::map<std::string,BindingTypeInfo> binding_type_catalog; }
#endif
namespace meowody {
using namespace godot;
namespace {
std::string text(const String& s) { return s.utf8().get_data(); }
std::filesystem::path path_of(const String& s) { auto value=text(s); return std::filesystem::path(std::u8string(value.begin(),value.end())); }
std::string quote(const String& s) { return "\""+text(s.c_escape())+"\""; }
bool resolve_type(const String& name, BindingTypeInfo& result) {
    auto found=binding_type_catalog.find(text(name));
    if(found!=binding_type_catalog.end()) { result=found->second; return true; }
    auto* db=ClassDBSingleton::get_singleton();
    if(!db || !db->class_exists(name)) return false;
    auto header=name.to_snake_case().replace("_2d", "2d").replace("_3d", "3d").replace("_1d", "1d");
    result={"godot::"+text(name),"godot_cpp/classes/"+text(header)+".hpp"};
    return true;
}
bool write_files(const std::map<std::filesystem::path,std::string>& files,String& error) {
    std::map<std::filesystem::path,std::string> originals; std::set<std::filesystem::path> absent; std::vector<std::filesystem::path> changed;
    try {
        for(auto& [path,content]:files) {
            if(std::filesystem::exists(path)) { std::ifstream in(path,std::ios::binary); std::string old((std::istreambuf_iterator<char>(in)),{}); if(old==content) continue; originals[path]=old; } else absent.insert(path);
            std::filesystem::create_directories(path.parent_path()); auto temporary=path; temporary+=".binding-tmp";
            std::ofstream out(temporary,std::ios::binary|std::ios::trunc); out<<content; out.flush(); if(!out) throw std::runtime_error("Cannot stage generated file"); changed.push_back(path);
        }
        for(auto& path:changed) { auto temporary=path; temporary+=".binding-tmp"; std::filesystem::copy_file(temporary,path,std::filesystem::copy_options::overwrite_existing); std::filesystem::remove(temporary); }
        return true;
    } catch(const std::exception& e) {
        for(auto& path:changed) { std::error_code ec; if(absent.contains(path)) std::filesystem::remove(path,ec); else { std::ofstream out(path,std::ios::binary|std::ios::trunc); out<<originals[path]; } auto temp=path; temp+=".binding-tmp"; std::filesystem::remove(temp,ec); }
        error=String("Generation failed: ")+e.what(); return false;
    }
}
}
String BindingGenerator::member_name(const String& input) {
    auto snake=text(input.to_snake_case()); std::string out="m_"; bool separator=false;
    for(unsigned char c:snake) {
        if((c>='a'&&c<='z')||(c>='0'&&c<='9')) { if(separator && out.size()>2) out+='_'; separator=false; out+=char(c); }
        else separator=true;
    }
    if(out.size()==2) out+="node";
    return String(out.c_str());
}
String BindingGenerator::default_class_name(Node* root) {
    String filename=root ? root->get_scene_file_path().get_file().get_basename() : String();
    if(filename.is_empty()&&root) filename=root->get_name();
    auto words=member_name(filename).trim_prefix("m_").split("_"); String result;
    for(const auto& word:words) if(!word.is_empty()) result+=word.substr(0,1).to_upper()+word.substr(1);
    if(result.is_empty() || (result.unicode_at(0)>='0'&&result.unicode_at(0)<='9')) result="Prefab"+result;
    return result+"View";
}
bool BindingGenerator::valid_class_name(const String& value) {
    if(value.is_empty()) return false;
    auto s=text(value); if(!((s[0]>='A'&&s[0]<='Z')||(s[0]>='a'&&s[0]<='z'))) return false;
    for(auto c:s) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')) return false;
    static const std::set<std::string> keywords={"class","struct","namespace","auto","int","float","double","char","bool","void","long","short","signed","unsigned","const","volatile","static","extern","virtual","public","private","protected","template","typename","enum","union","return","if","else","for","while","do","switch","case","default","break","continue","goto","new","delete","this","operator","using","typedef","friend","inline","constexpr","consteval","constinit","mutable","nullptr","true","false","throw","try","catch","noexcept","sizeof","alignof","alignas","decltype","static_assert","thread_local","explicit","export","register","asm","typeid","dynamic_cast","static_cast","reinterpret_cast","const_cast","requires","concept","co_await","co_yield","co_return","and","or","not","xor","and_eq","or_eq","not_eq","xor_eq","bitand","bitor","compl","wchar_t","char8_t","char16_t","char32_t"};
    return value.is_valid_identifier() && !keywords.contains(s);
}
std::vector<NativeBinding> BindingGenerator::collect(Node* root,String& error) {
    std::vector<NativeBinding> bindings; std::set<String> names;
    std::function<void(Node*)> visit=[&](Node* node) {
        if(node!=root && bool(node->get_meta(BIND_MARK,false))) {
            String stem=member_name(node->get_name()),name=stem; int suffix=0;
            while(!names.insert(name).second) name=stem+"_"+String::num_int64(++suffix);
            BindingTypeInfo type;
            if(!resolve_type(node->get_class(),type)) { error="Unknown reflected C++ node type: "+node->get_class(); return; }
            bindings.push_back({node,name,String(type.cpp.c_str()),String(type.header.c_str())});
        }
        if(node!=root && node->has_meta(BIND_CLASS)) return;
        for(int i=0;i<node->get_child_count() && error.is_empty();++i) visit(node->get_child(i));
    };
    if(root) visit(root); else error="No scene root"; return bindings;
}
BindingResult BindingGenerator::generate(Node* root) {
    BindingResult result; if(!root || root->get_scene_file_path().is_empty()) { result.error="Save the scene before generating"; return result; }
    String cls=root->get_meta(BIND_CLASS,default_class_name(root));
    String base=root->get_meta(BIND_BASE,root->get_class());
    if(!valid_class_name(cls) || cls==base) { result.error="Invalid or conflicting native class name"; return result; }
    BindingTypeInfo base_type;
    if(!resolve_type(base,base_type)) { result.error="Unknown native base class: "+base; return result; }
    auto bindings=collect(root,result.error); if(!result.error.is_empty()) return result;
    String project=ProjectSettings::get_singleton()->globalize_path("res://");
    String output=root->get_meta(BIND_OUTPUT,"../src/ui/generated");
    if(output.begins_with("res://")) output=ProjectSettings::get_singleton()->globalize_path(output);
    auto directory=path_of(output.is_absolute_path()?output:project.path_join(output)).lexically_normal();
    auto source=path_of(project.path_join("../src")).lexically_normal(); auto relative=directory.lexically_relative(source);
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..") { result.error="Choose an output directory inside the framework src directory"; return result; }
    std::set<std::string> includes{base_type.header}; std::string members;
    for(auto& b:bindings) {
        includes.insert(text(b.header));
        members+="    GDPROPERTY_HINT(PROPERTY_HINT_NODE_TYPE, "+quote(b.node->get_class())+") "+text(b.cpp_type)+"* "+text(b.member)+" = nullptr;\n";
    }
    bool ui_lifecycle=bool(root->get_meta("meowody_bind_ui",false));
    std::string name=text(cls),base_cpp=base_type.cpp;
    std::string include_text="// Generated includes.\n#pragma once\n";
    for(auto& header:includes) include_text+="#include \""+header+"\"\n";
    std::string member_text="// Generated members. Included inside the native class.\n";
    member_text+="    static constexpr const char* SCENE_PATH = "+quote(root->get_scene_file_path())+";\n"+members;
    std::map<std::filesystem::path,std::string> files;
    files[directory/(name+".includes.gen.h")]=include_text;
    files[directory/(name+".bindings.gen.inc")]=member_text;
    std::string lifecycle_header=ui_lifecycle ? "    GDMETHOD void on_open(const godot::Variant& data);\n    GDMETHOD void on_opened();\n    GDMETHOD void on_closing();\n    GDMETHOD void on_close();\n" : "";
    std::string lifecycle_cpp;
    if(ui_lifecycle) lifecycle_cpp="\nvoid "+name+"::on_open(const godot::Variant& data) {\n    // Apply this opening's data here.\n}\nvoid "+name+"::on_opened() {\n}\nvoid "+name+"::on_closing() {\n}\nvoid "+name+"::on_close() {\n}\n";
    auto user_header=directory/(name+".h");
    auto known=binding_type_catalog.find(name);
    if(known==binding_type_catalog.end() && ClassDBSingleton::get_singleton()->class_exists(cls)) {
        result.error="Native class name is already registered outside this generator: "+cls; return result;
    }
    if(known!=binding_type_catalog.end() && (source/path_of(String(known->second.header.c_str()))).lexically_normal()!=user_header) {
        result.error="Native class name is already used by another class: "+cls; return result;
    }
    if(std::filesystem::exists(user_header)) {
        std::ifstream in(user_header,std::ios::binary); std::string previous((std::istreambuf_iterator<char>(in)),{});
        if(previous.find("GDREFLECT_CLASS("+name+", "+base_cpp+")")==std::string::npos) { result.error="Existing user class has a different base; update it explicitly before regenerating"; return result; }
    } else {
        files[user_header]="#pragma once\n#include \""+name+".includes.gen.h\"\n#include \"reflection/annotations.h\"\n#include \"reflection/class_macros.h\"\n\nclass "+name+" : public "+base_cpp+" {\n    GDREFLECT_CLASS("+name+", "+base_cpp+")\npublic:\n#include \""+name+".bindings.gen.inc\"\n"+lifecycle_header+"};\n";
    }
    auto user_cpp=directory/(name+".cpp");
    if(!std::filesystem::exists(user_cpp)) files[user_cpp]="#include \""+name+".h\"\n"+lifecycle_cpp;
    result.ok=write_files(files,result.error); return result;
}
} // namespace meowody
