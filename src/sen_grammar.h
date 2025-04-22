// sen_grammar.h
#pragma once

#include <tao/pegtl.hpp>
#include <iomanip>
#include <string>
#include <vector>
#include <variant>
#include <map>

namespace sen {

static std::string unquote(const std::string s) {
    std::string result;
    std::istringstream strstr(s);
    strstr >> std::quoted(result);
    return result;
}

namespace grammar {
    using namespace tao::pegtl;

    // Whitespace and comments
    struct ws : star<space> {}; // Spaces and tabs only
    struct ws_with_newline : star<sor<space, one<'\n', '\r'>>> {}; // Spaces, tabs, newlines
    struct comment : seq<one<'#'>, until<eol>> {};
    struct ignored : sor<ws_with_newline, comment> {};

    // Basic tokens
    struct identifier : plus<sor<alnum, one<'_'>>> {};
    struct variable : seq<upper, star<alnum>> {};
    struct quoted_string : seq<one<'"'>, star<sor<alnum, space, one<'-', '_', '.'>>>, one<'"'>> {};
    struct mime_part : plus<sor<alnum, one<'-'>>> {};
    struct mime_wildcard : one<'*'> {};
    struct mime_type : sor<seq<mime_part, one<'/'>, sor<mime_part, mime_wildcard>>, mime_part> {};

    // Keywords
    struct keyword_use : TAO_PEGTL_KEYWORD("USE") {};
    struct keyword_as : TAO_PEGTL_KEYWORD("AS") {};
    struct keyword_has : TAO_PEGTL_KEYWORD("has") {};
    struct keyword_and : TAO_PEGTL_KEYWORD("AND") {};
    struct keyword_relate : TAO_PEGTL_KEYWORD("RELATE") {};
    struct keyword_with : TAO_PEGTL_KEYWORD("WITH") {};
    struct keyword_rule : TAO_PEGTL_KEYWORD("RULE") {};
    struct keyword_if : TAO_PEGTL_KEYWORD("IF") {};
    struct keyword_then : TAO_PEGTL_KEYWORD("THEN") {};
    struct keyword_context : TAO_PEGTL_KEYWORD("CONTEXT") {};

    // Attributes: role="parent of"
    struct attribute_key : seq<identifier, not_at<keyword_and>> {};
    struct attribute_value : quoted_string {};
    struct attribute : seq<attribute_key, ws_with_newline, one<'='>, ws_with_newline, attribute_value> {};
    struct attributes : list<attribute, seq<opt<one<','>>, ws_with_newline>> {};

    // USE clause: USE relation/book-quote AS quotes
    struct use_clause : seq<keyword_use, ws_with_newline, mime_type, ws_with_newline, keyword_as, ws_with_newline, identifier> {};

    // Relation: (A ~quotes B) or (A ~genealogy B AND role="parent of")
    struct relation_op : one<'~'> {};
    struct relation_name : sor<identifier, quoted_string> {};
    struct relation_attributes : opt<seq<ws_with_newline, keyword_and, ws_with_newline, attributes>> {};
    struct relation : seq<one<'('>, ws_with_newline, variable, ws_with_newline, relation_op, relation_name, ws_with_newline, variable, relation_attributes, ws_with_newline, one<')'>> {};

    // Predicate: A has gender="male"
    struct predicate : seq<variable, ws_with_newline, keyword_has, ws_with_newline, attribute_key, ws_with_newline, one<'='>, ws_with_newline, attribute_value> {};

    // Condition: relation or predicate, combined with AND
    struct condition : sor<relation, predicate> {};
    struct conditions : list<condition, seq<ws_with_newline, keyword_and, ws_with_newline>> {};

    // THEN RELATE: RELATE(B, A, "quotes") WITH key="value", ...
    struct relation_from : variable {};
    struct relation_to : variable {};
    struct relate_clause : seq<keyword_relate, ws_with_newline, one<'('>, ws_with_newline, relation_from, ws_with_newline, opt<one<','>>, ws_with_newline, relation_to, ws_with_newline, opt<one<','>>, ws_with_newline, relation_name, ws_with_newline, one<')'>, opt<seq<ws_with_newline, keyword_with, ws_with_newline, attributes>>> {};

    // Rule: RULE name { IF conditions THEN RELATE(...) WITH ... }
    struct rule_name : identifier {};
    struct rule_body : seq<keyword_if, ws_with_newline, conditions, ws_with_newline, keyword_then, ws_with_newline, relate_clause> {};
    struct rule : seq<keyword_rule, ws_with_newline, rule_name, ws_with_newline, opt<one<'{'>>, ws_with_newline, rule_body, ws_with_newline, opt<one<'}'>>> {};

    // Context: CONTEXT text/book { ... }
    struct context : seq<keyword_context, ws_with_newline, mime_type, ws_with_newline, opt<one<'{'>>, ws_with_newline, star<seq<rule, ws_with_newline>>, opt<one<'}'>>> {};

    // Top-level: USE clauses followed by CONTEXT blocks
    struct use_clauses : star<seq<use_clause, ws_with_newline>> {};
    struct contexts : star<seq<context, ws_with_newline>> {};
    struct grammar : seq<ignored, use_clauses, contexts, ignored, eof> {};
}

namespace actions {
    // Data structures
    struct attribute_t {
        std::string key;
        std::string value;
    };

    struct relation_t {
        std::string var1; // First variable (e.g., A)
        std::string relation_name; // e.g., quotes, genealogy
        std::string var2; // Second variable (e.g., B)
        std::vector<attribute_t> attributes; // e.g., role="parent of"
    };

    struct predicate_t {
        std::string var; // e.g., A
        std::string key; // e.g., gender
        std::string value; // e.g., male
    };

    struct condition_t {
        std::variant<relation_t, predicate_t> value;
    };

    struct relate_t {
        std::string var1; // e.g., B
        std::string var2; // e.g., A
        std::string relation_name; // e.g., genealogy
        std::vector<attribute_t> attributes;
    };

    struct rule_t {
        std::string name; // e.g., child_of
        std::vector<condition_t> conditions;
        relate_t conclusion;
    };

    struct context_t {
        std::string mime_type; // e.g., text/book
        std::vector<rule_t> rules;
    };

    struct rule_state {
        std::map<std::string, std::string> aliases; // alias -> relation (e.g., quotes -> relation/book-quote)
        std::vector<context_t> contexts;
        context_t current_context;
        rule_t current_rule;
        condition_t current_condition;
        relate_t current_relate;
        attribute_t current_attribute;
        std::vector<std::string> current_vars;   // stack of parsed relation vars
        std::string rule_name;      // Store rule identifier
        std::string current_name;   // For identifiers, relation names, etc.
        std::string relation_from;  // Store RELATE first variable
        std::string relation_to;    // Store RELATE second variable
        std::string relation_name;  // Store RELATE relation name
        std::vector<attribute_t> current_attributes; // Temporary for relation attributes

        void reset() {
            aliases.clear();
            contexts.clear();
            current_context = context_t{};
            current_rule = rule_t{};
            current_condition = condition_t{};
            current_relate = relate_t{};
            current_attribute = attribute_t{};
            current_vars.clear();
            rule_name.clear();
            current_name.clear();
            relation_from.clear();
            relation_to.clear();
            relation_name.clear();
            current_attributes.clear();
        }
    };

    // Actions
    template<typename Rule>
    struct action : tao::pegtl::nothing<Rule> {};

    template<> struct action<grammar::mime_type> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_context.mime_type = in.string();
            std::cout << "Parsed MIME type: " << state.current_context.mime_type << "\n";
        }
    };

    template<> struct action<grammar::identifier> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_name = in.string();
            std::cout << "Parsed identifier: " << state.current_name << "\n";
        }
    };

    template<> struct action<grammar::variable> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_vars.push_back(in.string());
            std::cout << "Parsed variable: " << in.string() << "\n";
        }
    };

    template<> struct action<grammar::quoted_string> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_name = unquote(in.string());
            std::cout << "Parsed quoted string: " << state.current_name << "\n";
        }
    };

    template<> struct action<grammar::use_clause> {
        template<typename ActionInput>
        static void apply(const ActionInput&, rule_state& state) {
            state.aliases[state.current_name] = state.current_context.mime_type;
            std::cout << "Added alias: " << state.current_name << " -> " << state.current_context.mime_type << "\n";
            state.current_name.clear();
            state.current_context.mime_type.clear();
        }
    };

    template<> struct action<grammar::keyword_context> {
        static void apply0(rule_state&) {
            std::cout << "Parsed CONTEXT keyword\n";
        }
    };

    template<> struct action<grammar::context> {
        static void apply0(rule_state& state) {
            std::cout << "Adding context: " << state.current_context.mime_type << " with " << state.current_context.rules.size() << " rules\n";
            state.contexts.push_back(std::move(state.current_context));
            state.current_context = context_t{};
        }
    };

    template<> struct action<grammar::keyword_rule> {
        static void apply0(rule_state&) {
            std::cout << "Parsed RULE keyword\n";
        }
    };

    template<> struct action<grammar::rule_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.rule_name = in.string();
            std::cout << "Parsed rule_name '" << state.rule_name << "' at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::rule> {
        static void apply0(rule_state& state) {
            if (!state.rule_name.empty()) {
                state.current_rule.name = std::move(state.rule_name);
                std::cout << "Added rule: " << state.current_rule.name << "\n";
                state.current_context.rules.push_back(std::move(state.current_rule));
                state.current_rule = rule_t{};
                state.rule_name.clear();
                state.current_name.clear();
            } else {
                std::cout << "Failed to add rule, no name set\n";
            }
        }
    };

    template<> struct action<grammar::keyword_if> {
        static void apply0(rule_state&) {
            std::cout << "Parsed IF keyword\n";
        }
    };

    template<> struct action<grammar::keyword_then> {
        static void apply0(rule_state&) {
            std::cout << "Parsed THEN keyword\n";
        }
    };

    template<> struct action<grammar::keyword_relate> {
        static void apply0(rule_state&) {
            std::cout << "Parsed RELATE keyword\n";
        }
    };

    template<> struct action<grammar::keyword_with> {
        static void apply0(rule_state&) {
            std::cout << "Parsed WITH keyword\n";
        }
    };

    template<> struct action<grammar::rule_body> {
        static void apply0(rule_state&) {
            std::cout << "Parsed rule_body\n";
        }
    };

    template<> struct action<grammar::relation_name> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.relation_name = state.current_name;
            std::cout << "Parsed relation_name: " << state.relation_name << " at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::relation_from> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.relation_from = in.string();
            std::cout << "Parsed relation_from: " << state.relation_from << " at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::relation_to> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.relation_to = in.string();
            std::cout << "Parsed relation_to: " << state.relation_to << " at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::attribute_key> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_attribute.key = in.string();
            std::cout << "Parsed attribute key: " << state.current_attribute.key << "\n";
        }
    };

    template<> struct action<grammar::attribute_value> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_attribute.value = unquote(in.string());
            std::cout << "Parsed attribute value: " << state.current_attribute.value << "\n";
        }
    };

    template<> struct action<grammar::attribute> {
        static void apply0(rule_state& state) {
            if (!state.current_attribute.key.empty() && !state.current_attribute.value.empty()) {
                std::cout << "Added attribute: " << state.current_attribute.key << "=" << state.current_attribute.value << "\n";
                state.current_attributes.push_back(std::move(state.current_attribute));
            } else {
                std::cout << "Failed to add attribute: key=" << state.current_name << ", value=" << state.current_attribute.value << "\n";
            }
            state.current_attribute = attribute_t{};
        }
    };

    template<> struct action<grammar::predicate> {
    	template<typename ActionInput>
        static void apply(const ActionInput&, rule_state& state) {
            predicate_t pred;
            pred.var = state.current_vars.back();
            state.current_vars.pop_back();
            pred.key = std::move(state.current_attribute.key);
            pred.value = std::move(state.current_attribute.value);
            state.current_condition.value = pred;
            std::cout << "Parsed predicate: " << pred.var << " has " << pred.key << "=\"" << pred.value << "\"\n";
            state.current_vars.clear();
            state.current_attribute = attribute_t{};
        }
    };

    template<> struct action<grammar::condition> {
        static void apply0(rule_state& state) {
            state.current_rule.conditions.push_back(std::move(state.current_condition));
            std::cout << "Added condition to rule\n";
            state.current_condition = condition_t{};
        }
    };

    template<> struct action<grammar::relate_clause> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            std::cout << "Parsing relate_clause at: " << in.position() << "\n";
            state.current_relate.var1 = std:: move(state.relation_from);
            state.current_relate.var2 = std:: move(state.relation_to);
            state.current_relate.relation_name = std:: move(state.relation_name);
            state.current_relate.attributes = std::move(state.current_attributes);

            std::cout << "Parsed relate clause: RELATE(" << state.current_relate.var1 << ", " << state.current_relate.var2
                      << ", " << state.current_relate.relation_name << ") WITH attrs ";

            for (size_t i = 0; i < state.current_relate.attributes.size(); ++i) {
                std::cout << state.current_relate.attributes[i].key << "=\"" << state.current_relate.attributes[i].value << "\"";
                if (i < state.current_relate.attributes.size() - 1) std::cout << ", ";
            }
            std::cout << "\n";

            state.current_rule.conclusion = std::move(state.current_relate);
            state.relation_from.clear();
            state.relation_to.clear();
            state.relation_name.clear();
            state.current_attributes.clear();
            state.current_relate = relate_t{};
        }
    };
}
}
