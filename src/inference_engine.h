#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tao/pegtl.hpp>
#include <stdexcept>
#include <sstream>
#include <iostream>

#include <tao/pegtl/contrib/analyze.hpp>
#include <tao/pegtl/contrib/trace.hpp>

using namespace tao::pegtl;

struct Entity {
    std::string id;
    std::unordered_map<std::string, std::string> properties;
};

struct Relation {
    std::string from;
    std::string to;
    std::string type;
    std::unordered_map<std::string, std::string> properties;
    bool inferred = false;
};

struct ConditionData {
    std::string type;  // "relation", "has", "property"
    std::string relation;  // For ~relation
    std::vector<std::string> vars;  // A, B, etc.
    std::unordered_map<std::string, std::string> properties;  // For has/property
};

struct RuleData {
    std::string name;
    std::vector<ConditionData> conditions;
    std::string from, to, relation;
    std::unordered_map<std::string, std::string> relProperties;
    std::string currentKey;
    std::vector<std::string> vars;
};

struct ContextData {
    std::string mimeType;
    std::vector<RuleData> rules;
};

struct ParseState {
    std::vector<ContextData> contexts;  // Finalized contexts
    std::unordered_map<std::string, std::string> aliases;
    RuleData currentRule;
    ContextData currentContext;  // Transient context being built
    std::string alias;           // Transient Alias     for USE or CONTEXT
    std::string mimeType;        // Transient MIME type for USE or CONTEXT
};

class KnowledgeBase {
public:
    std::vector<Entity> entities;
    std::vector<Relation> relations;

    void addEntity(const std::string& id, const std::unordered_map<std::string, std::string>& props) {
        if (std::find_if(entities.begin(), entities.end(),
                         [&](const Entity& e) { return e.id == id; }) == entities.end()) {
            entities.push_back({id, props});
        }
    }

    void addRelation(const std::string& from, const std::string& to, const std::string& type,
                     const std::unordered_map<std::string, std::string>& props = {}, bool inferred = false) {
        relations.push_back({from, to, type, props, inferred});
    }
};

class InferenceEngine {
public:
    InferenceEngine(KnowledgeBase& knowledge, int depth);
    void loadDSL(const std::string& dsl);
    void loadFromSource(const std::string& sourceId);
    void infer(const std::string& sourceMimeType);

private:
    void loadEntityAndRelations(const std::string& id, int depth, std::unordered_set<std::string>& loadedIds);
    void loadEntity(const std::string& id);
    std::vector<Relation> getRelationsForId(const std::string& id);
    std::unordered_map<std::string, std::string> queryEntityAttributes(const std::string& senId);
    std::vector<std::string> querySENto(const std::string& senId);
    std::string queryRelationType(const std::string& fromId, const std::string& toId);
    std::unordered_map<std::string, std::string> queryRelationProperties(const std::string& fromId, const std::string& toId);
    void applyRule(const RuleData& rule);
    bool checkTransitive(const std::string& e1, const std::string& e2, const std::string& e3,
                                      const std::vector<ConditionData>& conditions,
                                      std::unordered_map<std::string, std::string>& varMap);
    bool checkRelation(const std::string& from, const std::string& to, const std::string& type,
                                    const std::vector<ConditionData>& conditions);

    KnowledgeBase kb;
    int maxDepth;
    ParseState state;
};

// Grammar rules accepting liberal whitespace use, line comments and is case insensitive
// Helpers
struct ws : sor<space/*, line_comment*/> {};
struct wss : star<ws> {};
struct wsp : plus<ws> {};
struct wsb : star<sor<wsp, eol>> {};
struct Special : one<'-', '.'> {};
struct String : seq<one<'"'>, plus<not_at<one<'"'>>, any>, one<'"'>> {};
struct Identifier : plus<sor<alnum, Special, one<'_'>>> {};

// MIME Type
struct mime_component : sor<one<'*'>, plus<sor<alnum, Special>>> {};
struct mime_type : seq<mime_component, one<'/'>, mime_component> {};

// Use Declaration
struct USE : TAO_PEGTL_ISTRING("use") {};
struct AS : TAO_PEGTL_ISTRING("as") {};
struct alias : Identifier {};
struct use_decl : seq<USE, wsp, mime_type, wsp, AS, wsp, alias> {};

// DSL Core
struct var : Identifier {};
struct property_key : Identifier {};
struct property_value : String {};
struct property : seq<property_key, one<'='>, property_value> {};
struct property_list : star<seq<one<','>, wsp, property>> {};
struct HAS : TAO_PEGTL_ISTRING("has") {};
struct has_clause : seq<var, wsp, HAS, wsp, property, property_list> {};
struct RELATE : TAO_PEGTL_ISTRING("relate") {};
struct relate_args : seq<var, wss, one<','>, wss, var, wss, one<','>, wss, String> {};
struct relation : seq<var, wsp, one<'~'>, alias, wsp, var> {};
struct condition : sor<relation, property> {};
struct AND : TAO_PEGTL_ISTRING("and") {};
struct and_clause : seq<wsp, AND, wsp, sor<has_clause, condition>> {};
struct IF : TAO_PEGTL_ISTRING("if") {};
struct if_clause : seq<IF, wsp, one<'('>, wss, sor<has_clause, condition>, star<and_clause>, wss, one<')'>> {};
struct WITH : TAO_PEGTL_ISTRING("with") {};
struct with_clause : seq<wsp, WITH, wsp, property, property_list> {};
struct THEN : TAO_PEGTL_ISTRING("then") {};
struct then_clause : seq<THEN, wsp, RELATE, wss, one<'('>, relate_args, wss, one<')'>, opt<with_clause>> {};
struct rule_name : Identifier {};
struct RULE : TAO_PEGTL_ISTRING("rule") {};
struct rule_body : seq<if_clause, wsb, then_clause> {};
struct rule : seq<RULE, wsp, rule_name, wsp, one<'{'>, wsb, rule_body, wsb, one<'}'>> {};
struct CONTEXT : TAO_PEGTL_ISTRING("context") {};
struct context_start : seq<CONTEXT, wsp, mime_type> {};
struct context_def : seq<context_start, wsp, one<'{'>, wsb, plus<rule, wsb>, one<'}'>> {};
struct grammar : seq<wsb, star<use_decl, wsb>, wsb, plus<context_def, wsb>, eof> {};

// Actions
template<typename rule> struct Action {};

template<> struct Action<mime_type> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        state.mimeType = in.string();  // Store MIME type transiently
    }
};

template<> struct Action<use_decl> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);
        std::cout << "USE alias '" << state.alias << "' for type '" << state.mimeType << "'\n";
        state.aliases[state.alias] = state.mimeType;
        state.alias.clear();
        state.mimeType.clear();
    }
};

template<> struct Action<alias> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        state.alias = in.string();  // Alias for USE or RELATE
    }
};

template<> struct Action<context_start> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);
        state.currentContext = {state.mimeType, {}};  // Start transient context
        std::cout << "starting context '" << state.mimeType << "'\n";
    }
};

template<> struct Action<rule_name> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        state.currentRule.name = in.string();
    }
};

template<> struct Action<var> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        std::string var = in.string();
        if (var.size() >= 2 && var.front() == '"' && var.back() == '"') {
            var = var.substr(1, var.size() - 2);  // Strip quotes from relation
        }
        state.currentRule.vars.push_back(var);  // Temp store for relation/has
    }
};

template<> struct Action<property_key> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        state.currentRule.currentKey = in.string();
    }
};

template<> struct Action<property_value> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        std::string value = in.string();
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);  // Strip quotes
        }
        if (state.currentRule.relation.empty()) {  // IF clause properties
            if (!state.currentRule.currentKey.empty()) {
                std::cout << "  >> adding value '" << value
                          << "' to existing property '" << state.currentRule.currentKey << "'" << std::endl;
                state.currentRule.conditions.back().properties[state.currentRule.currentKey] = value;
            } else {
                state.currentRule.conditions.push_back({"property", "", {}, {{state.currentRule.currentKey, value}}});
            }
        } else {  // THEN clause WITH properties
            std::cout << " >> add relation property " << state.currentRule.currentKey
                      << " = " << value << std::endl;
            state.currentRule.relProperties[state.currentRule.currentKey] = value;
        }
        state.currentRule.currentKey.clear();
    }
};

template<> struct Action<has_clause> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);

        state.currentRule.conditions.push_back({"has", "", {state.currentRule.vars[0]}, {}});
        state.currentRule.vars.clear();
    }
};

template<> struct Action<relate_args> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);
        std::cout << "relation args raw: " << in.string() << std::endl;

        std::string from = state.currentRule.vars[0];
        std::string to  = state.currentRule.vars[1];
        std::string rel = state.alias; //state.currentRule.relation;
        state.currentRule.relation = rel;   // important for properties parsing

        std::cout << "adding relation '" << rel << "'"
                  << " [" << from << "->" << to << "]" << std::endl;

        state.currentRule.conditions.push_back({"relation", rel, {from, to}, {}});
        state.currentRule.vars.clear();
        state.alias.clear();
    }
};

template<> struct Action<rule> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);

        std::cout << "added rule '" << state.currentRule.name << "' from context '"
                  << state.currentContext.mimeType << "', with relation properties: ";
        for (const auto& [k, v] : state.currentRule.relProperties) {
            std::cout << k << "=\"" << v << "\" ";
        }
        std::cout << "\nwith conditions:";
        // DEBUG print conditions
        for (const auto& cond : state.currentRule.conditions) {
            std::cout << "type=" << cond.type << ", vars:"
                      << cond.vars[0] << ", " << cond.vars[1];
            if (cond.type == "relation")
                std::cout << "  relation: " << cond.relation;

            std::cout << std::endl << "  properties: ";

            for (const auto& [k, v] : cond.properties) {
                std::cout << k << "=\"" << v << "\" ";
            }
            std::cout << std::endl;
        }

        state.currentContext.rules.push_back(state.currentRule);  // Add to transient context
        //state.currentRule = RuleData();  // Reset rule data
    }
};

template<> struct Action<context_def> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, ParseState& state) {
        assert(&in);

        if (!state.currentContext.mimeType.empty()) {
            std::cout << "finishing context '" << state.currentContext.mimeType << "'\n";
            state.contexts.push_back(state.currentContext);  // Commit transient context
        }
        state.currentContext = ContextData();  // Reset transient context
        state.currentRule = RuleData();        // Reset rule
        state.mimeType.clear();                // Reset transient MIME type
        state.alias.clear();
    }
};
