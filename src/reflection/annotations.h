#pragma once

#if defined(GDREFLECT_CODEGEN) && defined(__clang__)
#define GDREFLECT_ANNOTATE(Name) [[clang::annotate(Name)]]
#else
#define GDREFLECT_ANNOTATE(Name)
#endif

#define GDPROPERTY GDREFLECT_ANNOTATE("gdproperty")
#define GDPROPERTY_NAME(NameLiteral) GDREFLECT_ANNOTATE("gdproperty|name=" NameLiteral)
#define GDPROPERTY_ACCESSORS(Getter, Setter) GDREFLECT_ANNOTATE("gdproperty|getter=" #Getter "|setter=" #Setter)
// Generates the method body; use GDSIGNAL for manually implemented signals.
#define GDEMIT_SIGNAL GDREFLECT_ANNOTATE("gdsignal|emit=true")
#define GDPROPERTY_HINT(Hint, ...) \
    GDREFLECT_ANNOTATE("gdproperty|hint=" #Hint "|hint_string=" "" __VA_ARGS__)
#define GDPROPERTY_RANGE(Min, Max, Step) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_RANGE|hint_string=" #Min "," #Max "," #Step)
#define GDPROPERTY_ENUM(OptionsLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_ENUM|hint_string=" OptionsLiteral)
#define GDPROPERTY_FLAGS(OptionsLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_FLAGS|hint_string=" OptionsLiteral)
#define GDPROPERTY_USAGE(UsageExpression) \
    GDREFLECT_ANNOTATE("gdproperty|usage=" #UsageExpression)
#define GDPROPERTY_CATEGORY(CategoryLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|category=" CategoryLiteral)
#define GDPROPERTY_GROUP(GroupLiteral, PrefixLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|group=" GroupLiteral "|prefix=" PrefixLiteral)
#define GDPROPERTY_SUBGROUP(GroupLiteral, PrefixLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|subgroup=" GroupLiteral "|subgroup_prefix=" PrefixLiteral)
#define GDPROPERTY_FILE(FilterLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_FILE|hint_string=" FilterLiteral)
#define GDPROPERTY_DIR GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_DIR")
#define GDPROPERTY_GLOBAL_FILE(FilterLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_GLOBAL_FILE|hint_string=" FilterLiteral)
#define GDPROPERTY_GLOBAL_DIR GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_GLOBAL_DIR")
#define GDPROPERTY_MULTILINE GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_MULTILINE_TEXT")
#define GDPROPERTY_PLACEHOLDER(TextLiteral) \
    GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_PLACEHOLDER_TEXT|hint_string=" TextLiteral)
#define GDPROPERTY_PASSWORD GDREFLECT_ANNOTATE("gdproperty|hint=PROPERTY_HINT_PASSWORD")
#define GDMETHOD GDREFLECT_ANNOTATE("gdmethod")
#define GDRPC(RpcMode, TransferMode, CallLocal, Channel) \
    GDREFLECT_ANNOTATE("gdrpc|rpc_mode=" #RpcMode "|transfer_mode=" #TransferMode "|call_local=" #CallLocal "|channel=" #Channel)
#define GDSIGNAL GDREFLECT_ANNOTATE("gdsignal")
