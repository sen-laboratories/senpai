#pragma once

#include <tao/pegtl.hpp>
#include <string>
#include <vector>
#include <variant>
#include <map>
#include <iostream>

namespace sen {

namespace grammar {
    using namespace tao::pegtl;

    // Utility
    inline std::string unquote(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    // Whitespace and comments
    struct ws : star<space> {};
    struct ws_with_newline : star<sor<space, one<'\n', '\r'>>> {};
    struct comment : seq<one<'#'>, until<eol>> {};
    struct ignored : sor<ws_with_newline, comment> {};

    // Keywords
    struct keyword_use : TAO_PEGTL_ISTRING("USE") {};
    struct keyword_as : TAO_PEGTL_ISTRING("AS") {};
    struct keyword_has : TAO_PEGTL_ISTRING("HAS") {};
    struct keyword_and : TAO_PEGTL_ISTRING("AND") {};
    struct keyword_relate : TAO_PEGTL_ISTRING("RELATE") {};
    struct keyword_with : TAO_PEGTL_ISTRING("WITH") {};
    struct keyword_rule : TAO_PEGTL_ISTRING("RULE") {};
    struct keyword_if : TAO_PEGTL_ISTRING("IF") {};
    struct keyword_then : TAO_PEGTL_ISTRING("THEN") {};
    struct keyword_context : TAO_PEGTL_ISTRING("CONTEXT") {};

    // Basic tokens
    struct identifier : plus<sor<alnum, one<'_'>>> {};
    struct variable : seq<upper, star<alnum>> {};
    struct quoted_string : seq<one<'"'>, star<sor<alnum, space, one<'-', '_', '.'>>>, one<'"'>> {};

    // MIME type
    struct mime_part : plus<sor<alnum, one<'-'>>> {};
    struct mime_wildcard : one<'*'> {};
    struct mime_type : seq<sor<mime_part, mime_wildcard>, one<'/'>, sor<mime_part, mime_wildcard> > {};

    // Attributes
    struct attribute_key : identifier {};
    struct attribute_value : quoted_string {};
    struct attribute : seq<attribute_key, ws_with_newline, one<'='>, ws_with_newline, attribute_value> {};
    struct attributes : list<attribute, seq<opt<one<','>>, ws_with_newline>> {};

    // USE clause
    struct use_clause : seq<keyword_use, ws_with_newline, mime_type, ws_with_newline, keyword_as, ws_with_newline, identifier> {};

    // Relation
    struct relation_op : one<'~'> {};
    struct relation_name : sor<identifier, quoted_string> {};
    struct relation_attributes : star<seq<ws_with_newline, keyword_and, ws_with_newline, attributes>> {};
    struct relation : seq<variable, ws_with_newline, relation_op, relation_name, ws_with_newline, variable, relation_attributes> {};

    // Predicate
    struct predicate : seq<variable, ws_with_newline, keyword_has, ws_with_newline, attribute_key, ws_with_newline, one<'='>, ws_with_newline, attribute_value> {};

    // Condition
    struct condition : sor<relation, predicate> {};
    struct conditions : seq<one<'('>, ws_with_newline,
                                condition,
                                star<seq<ws_with_newline, keyword_and, ws_with_newline, condition, ws_with_newline>>,
                            one<')'> > {};

    // THEN RELATE
    struct relation_from : variable {};
    struct relation_to : variable {};
    struct relate_clause : seq<keyword_relate, ws_with_newline, one<'('>, ws_with_newline,
                            relation_from, ws_with_newline, opt<one<','>>, ws_with_newline,
                            relation_to, ws_with_newline, opt<one<','>>, ws_with_newline,
                            relation_name, ws_with_newline, one<')'>, opt<seq<ws_with_newline,
                            keyword_with, ws_with_newline, attributes>>> {};

    // Rule
    struct rule_name : identifier {};
    struct rule_body : seq<keyword_if, ws_with_newline, conditions, ws_with_newline, keyword_then, ws_with_newline, relate_clause> {};
    struct rule : seq<keyword_rule, ws_with_newline, rule_name, ws_with_newline, opt<one<'{'>>, ws_with_newline, rule_body, ws_with_newline, opt<one<'}'>>> {};

    // Context
    struct context : seq<keyword_context, ws_with_newline, mime_type, ws_with_newline, opt<one<'{'>>, ws_with_newline, star<seq<rule, ws_with_newline>>, opt<one<'}'>>> {};

    // Top-level
    struct use_clauses : star<seq<use_clause, ws_with_newline>> {};
    struct contexts : star<seq<context, ws_with_newline>> {};
    struct grammar : seq<ignored, use_clauses, contexts, ignored, eof> {};
}

namespace actions {
    // Data structures
    struct attribute_t {
        std::string key;
        std::string value;

        bool operator==(const attribute_t& a) const
        {
            return (key == a.key && value == a.value);
        }

        bool operator!=(const attribute_t& a) const
        {
            return (key != a.key || value != a.value);
        }
    };

    struct relation_t {
        std::string var1;
        std::string relation_name;
        std::string var2;
        std::vector<attribute_t> attributes;
    };

    struct predicate_t {
        std::string var;
        std::string key;
        std::string value;
    };

    struct condition_t {
        std::variant<relation_t, predicate_t> value;
    };

    struct relate_t {
        std::string var1;
        std::string var2;
        std::string relation_name;
        std::vector<attribute_t> attributes;
    };

    struct rule_t {
        std::string name;
        std::vector<condition_t> conditions;
        relate_t conclusion;
    };

    struct context_t {
        std::string mime_type;
        std::vector<rule_t> rules;
    };

    struct rule_state {
        std::map<std::string, std::string> aliases;
        std::vector<context_t> contexts;
        context_t current_context;
        rule_t current_rule;
        condition_t current_condition;
        relate_t current_relate;
        attribute_t current_attribute;
        std::string rule_name;
        std::string current_name;
        std::string relation_name;
        std::vector<std::string> current_vars;
        std::vector<attribute_t> current_attributes;

        void reset() {
            aliases.clear();
            contexts.clear();
            current_context = context_t{};
            current_rule = rule_t{};
            current_condition = condition_t{};
            current_relate = relate_t{};
            current_attribute = attribute_t{};
            rule_name.clear();
            current_name.clear();
            relation_name.clear();
            current_vars.clear();
            current_attributes.clear();
        }
    };

    // Actions
    template<typename Rule>
    struct action : tao::pegtl::nothing<Rule> {};

    template<> struct action<grammar::mime_type> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_context.mime_type = grammar::unquote(in.string());
            std::cout << "Parsed MIME type: " << state.current_context.mime_type << "\n";
        }
    };

    template<> struct action<grammar::identifier> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_name = grammar::unquote(in.string());
            std::cout << "Parsed identifier: " << state.current_name << "\n";
        }
    };

    template<> struct action<grammar::variable> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_vars.push_back(grammar::unquote(in.string()));
            std::cout << "Parsed variable: " << state.current_vars.back() << "\n";
        }
    };

    template<> struct action<grammar::quoted_string> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_name = grammar::unquote(in.string());
            std::cout << "Parsed quoted string: " << state.current_name << "\n";
        }
    };

    template<> struct action<grammar::use_clause> {
        template<typename ActionInput>
        static void apply(const ActionInput&, rule_state& state) {
            state.aliases[state.current_name] = state.current_context.mime_type;
            std::cout << "ADD ALIAS: " << state.current_name << " -> " << state.current_context.mime_type << "\n";
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
            std::cout << "ADD CONTEXT: " << state.current_context.mime_type << " with " << state.current_context.rules.size() << " rules\n";
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
            state.rule_name = grammar::unquote(in.string());
            std::cout << "Parsed rule_name '" << state.rule_name << "' at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::rule> {
        static void apply0(rule_state& state) {
            if (!state.rule_name.empty()) {
                state.current_rule.name = std::move(state.rule_name);
                std::cout << "ADD RULE: " << state.current_rule.name << ", conditions=" << state.current_rule.conditions.size()
                          << ", conclusion=" << state.current_rule.conclusion.relation_name << "\n";

                state.current_context.rules.push_back(std::move(state.current_rule));
                std::cout << "total rules: " << state.current_context.rules.size() << " rules.\n";

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

    template<> struct action<grammar::keyword_and> {
        static void apply0(rule_state&) {
            std::cout << "Parsed AND keyword\n";
        }
    };

    template<> struct action<grammar::keyword_has> {
        static void apply0(rule_state&) {
            std::cout << "Parsed HAS keyword\n";
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
        static void apply0(rule_state& state) {
            std::cout << "Parsed rule_body\n";
            std::cout << "  Rule state: name=" << state.rule_name
                      << ", conditions=" << state.current_rule.conditions.size()
                      << ", conclusion=" << state.current_rule.conclusion.relation_name
                      << ", vars=[";
            for (size_t i = 0; i < state.current_vars.size(); ++i) {
                std::cout << state.current_vars[i];
                if (i < state.current_vars.size() - 1) std::cout << ", ";
            }
            std::cout << "]\n";
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
            state.current_vars.push_back(grammar::unquote(in.string()));
            std::cout << "Parsed relation_from: " << state.current_vars.back() << " at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::relation_to> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_vars.push_back(grammar::unquote(in.string()));
            std::cout << "Parsed relation_to: " << state.current_vars.back() << " at: " << in.position() << "\n";
        }
    };

    template<> struct action<grammar::attribute_key> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_attribute.key = grammar::unquote(in.string());
            std::cout << "Parsed attribute key: " << state.current_attribute.key << "\n";
        }
    };

    template<> struct action<grammar::attribute_value> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            state.current_attribute.value = grammar::unquote(in.string());
            std::cout << "Parsed attribute value: " << state.current_attribute.value << "\n";
        }
    };

    template<> struct action<grammar::attribute> {
        static void apply0(rule_state& state) {
            if (!state.current_attribute.key.empty() && !state.current_attribute.value.empty()) {
                std::cout << "ADD ATTRIBUTE: " << state.current_attribute.key << "=" << state.current_attribute.value << "\n";
                state.current_attributes.push_back(std::move(state.current_attribute));
            } else {
                std::cout << "Failed to add attribute: key=" << state.current_name << ", value=" << state.current_attribute.value << "\n";
            }
            state.current_attribute = attribute_t{};
        }
    };

    template<> struct action<grammar::relation> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            std::cout << "Parsing relation at: " << in.position() << "\n";
            relation_t rel;
            if (state.current_vars.size() >= 2) {
                rel.var1 = state.current_vars[0];
                rel.var2 = state.current_vars[1];
            }
            rel.relation_name = std::move(state.relation_name);
            rel.attributes = std::move(state.current_attributes);
            state.current_condition.value = rel;
            std::cout << "Parsed relation: " << rel.var1 << " ~" << rel.relation_name << " " << rel.var2;
            if (!rel.attributes.empty()) {
                std::cout << " WITH ";
                for (size_t i = 0; i < rel.attributes.size(); ++i) {
                    std::cout << rel.attributes[i].key << "=\"" << rel.attributes[i].value << "\"";
                    if (i < rel.attributes.size() - 1) std::cout << ", ";
                }
            }
            std::cout << "\n";
            state.current_vars.clear();
            state.current_name.clear();
            state.current_attributes.clear();
        }
    };

    template<> struct action<grammar::predicate> {
        static void apply0(rule_state& state) {
            predicate_t pred;
            if (!state.current_vars.empty())
                pred.var = state.current_vars[0];

            pred.key = std::move(state.current_attribute.key);
            pred.value = std::move(state.current_attribute.value);
            state.current_condition.value = pred;

            std::cout << "Parsed predicate: " << pred.var << " has " << pred.key << "=\"" << pred.value << "\"\n";
            state.current_vars.clear();
            state.current_name.clear();
            state.current_attribute = attribute_t{};
        }
    };

    template<> struct action<grammar::condition> {
        static void apply0(rule_state& state) {
            state.current_rule.conditions.push_back(std::move(state.current_condition));
            std::cout << "ADD CONDITION to rule\n";
            state.current_condition = condition_t{};
        }
    };

    template<> struct action<grammar::conditions> {
        static void apply0(rule_state& state) {
            std::cout << "Parsed conditions, count: " << state.current_rule.conditions.size() << "\n";
        }
    };

    template<> struct action<grammar::relate_clause> {
        template<typename ActionInput>
        static void apply(const ActionInput& in, rule_state& state) {
            std::cout << "Parsing relate_clause at: " << in.position() << "\n";
            if (state.current_vars.size() >= 2) {
                state.current_relate.var1 = state.current_vars[0];
                state.current_relate.var2 = state.current_vars[1];
            }
            state.current_relate.relation_name = std::move(state.relation_name);
            state.current_relate.attributes = std::move(state.current_attributes);
            state.current_rule.conclusion = state.current_relate;
            std::cout << "Set conclusion: RELATE(" << state.current_relate.var1 << ", " << state.current_relate.var2
                      << ", " << state.current_relate.relation_name << ") WITH ";
            for (const auto& attr : state.current_relate.attributes) {
                std::cout << attr.key << "=\"" << attr.value << "\"";
                if (&attr != &state.current_relate.attributes.back()) std::cout << ", ";
            }
            std::cout << "\n";
            state.current_vars.clear();
            state.relation_name.clear();
            state.current_attributes.clear();
            state.current_relate = relate_t{};
        }
    };
}   // namespace actions
}   // namespace sen
