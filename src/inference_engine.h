#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <iostream>
#include "sen_grammar.h"

namespace sen {
    class InferenceEngine {
    public:
        void parse(const std::string& input) {
            state_.reset();
            tao::pegtl::memory_input in(input, "rules");
            try {
                tao::pegtl::parse<grammar::grammar, actions::action>(in, state_);
                std::cout << "Parsed DSL successfully. Contexts: " << state_.contexts.size() << "\n";
                for (const auto& ctx : state_.contexts) {
                    std::cout << "  Context: " << ctx.mime_type << ", Rules: " << ctx.rules.size() << "\n";
                }
            } catch (const tao::pegtl::parse_error& e) {
                std::cerr << "Parse error: " << e.what() << "\n";
            }
        }

        void add_fact(const actions::relation_t& fact) {
            facts_.push_back(fact);
            std::cout << "Added fact: " << fact.var1 << "(" << fact.relation_name << ", " << fact.var2 << ")";
            if (!fact.attributes.empty()) {
                std::cout << " WITH ";
                for (size_t i = 0; i < fact.attributes.size(); ++i) {
                    std::cout << fact.attributes[i].key << "=\"" << fact.attributes[i].value << "\"";
                    if (i < fact.attributes.size() - 1) std::cout << ", ";
                }
            }
            std::cout << "\n";
        }

        void add_predicate(const actions::predicate_t& pred) {
            predicates_.push_back(pred);
            std::cout << "Added predicate: " << pred.var << " has " << pred.key << "=\"" << pred.value << "\"\n";
        }

        std::vector<actions::relation_t> infer(const std::optional<std::string>& context = std::nullopt,
                                              int max_depth = 2, int iterations = 1) {
            std::cout << "Starting inference: context=" << (context ? *context : "all")
                      << ", max_depth=" << max_depth << ", iterations=" << iterations << "\n";
            std::vector<actions::relation_t> all_new_relations;
            for (int iter = 0; iter < iterations; ++iter) {
                std::cout << "Iteration " << (iter + 1) << "\n";
                std::vector<actions::relation_t> new_relations;
                for (const auto& ctx : state_.contexts) {
                    bool context_match = !context || matches_context(ctx.mime_type, *context);
                    std::cout << "  Checking context: " << ctx.mime_type << " (match=" << context_match << ")\n";
                    if (!context_match) continue;
                    for (const auto& rule : ctx.rules) {
                        if (is_transitive_rule(rule) && exceeds_max_depth(rule, max_depth)) {
                            std::cout << "    Skipping transitive rule: " << rule.name << " (exceeds max_depth)\n";
                            continue;
                        }
                        std::cout << "    Applying rule: " << rule.name << "\n";
                        apply_rule(rule, new_relations);
                    }
                }
                if (new_relations.empty()) {
                    std::cout << "  No new relations in iteration " << (iter + 1) << ", stopping\n";
                    break;
                }
                all_new_relations.insert(all_new_relations.end(), new_relations.begin(), new_relations.end());
                facts_.insert(facts_.end(), new_relations.begin(), new_relations.end());
            }
            std::cout << "Inference complete. New relations: " << all_new_relations.size() << "\n";
            return all_new_relations;
        }

    private:
        bool check_condition(const actions::condition_t& cond, std::map<std::string, std::string>& bindings) {
            if (std::holds_alternative<actions::relation_t>(cond.value)) {
                const auto& query = std::get<actions::relation_t>(cond.value);

                std::string resolved_name = state_.aliases.count(query.relation_name) ? state_.aliases[query.relation_name] : query.relation_name;
                std::cout << "      Checking relation: " << query.var1 << " ~" << resolved_name << " " << query.var2;
                if (!query.attributes.empty()) {
                    std::cout << " WITH ";
                    for (size_t i = 0; i < query.attributes.size(); ++i) {
                        std::cout << query.attributes[i].key << "=\"" << query.attributes[i].value << "\"";
                        if (i < query.attributes.size() - 1) std::cout << ", ";
                    }
                }
                std::cout << "\n";

                for (const auto& fact : facts_) {
                    std::string fact_name = state_.aliases.count(fact.relation_name) ? state_.aliases[fact.relation_name] : fact.relation_name;
                    if (fact_name == resolved_name && fact.var2.size() == query.var2.size()) {
                        std::map<std::string, std::string> temp_bindings = bindings;
                        bool args_match = true;
                        if (temp_bindings.count(query.var1)) {
                            if (temp_bindings[query.var1] != fact.var1) {
                                std::cout << "        Binding mismatch: " << query.var1 << "=" << temp_bindings[query.var1] << " != " << fact.var1 << "\n";
                                args_match = false;
                            }
                        } else {
                            temp_bindings[query.var1] = fact.var1;
                        }
                        if (temp_bindings.count(query.var2)) {
                            if (temp_bindings[query.var2] != fact.var2) {
                                std::cout << "        Binding mismatch: " << query.var2 << "=" << temp_bindings[query.var2] << " != " << fact.var2 << "\n";
                                args_match = false;
                            }
                        } else {
                            temp_bindings[query.var2] = fact.var2;
                        }
                        bool attrs_match = true;
                        for (const auto& query_attr : query.attributes) {
                            auto it = std::find_if(fact.attributes.begin(), fact.attributes.end(),
                                [&](const auto& fa) { return fa.key == query_attr.key && fa.value == query_attr.value; });
                            if (it == fact.attributes.end()) {
                                std::cout << "        Attribute mismatch: " << query_attr.key << "=\"" << query_attr.value << "\" not found\n";
                                attrs_match = false;
                            }
                        }
                        if (args_match && attrs_match) {
                            bindings = std::move(temp_bindings);
                            std::cout << "        Match found: " << fact.var1 << " ~" << fact_name << " " << fact.var2;
                            if (!fact.attributes.empty()) {
                                std::cout << " WITH ";
                                for (size_t i = 0; i < fact.attributes.size(); ++i) {
                                    std::cout << fact.attributes[i].key << "=\"" << fact.attributes[i].value << "\"";
                                    if (i < fact.attributes.size() - 1) std::cout << ", ";
                                }
                            }
                            std::cout << "\n";
                            return true;
                        }
                    }
                }
                std::cout << "      No match for relation\n";
            } else if (std::holds_alternative<actions::predicate_t>(cond.value)) {
                const auto& query = std::get<actions::predicate_t>(cond.value);
                std::cout << "      Checking predicate: " << query.var << " has " << query.key << "=\"" << query.value << "\"\n";
                for (const auto& pred : predicates_) {
                    if (pred.var == query.var && pred.key == query.key && pred.value == query.value) {
                        std::cout << "        Match found: " << pred.var << " has " << pred.key << "=\"" << pred.value << "\"\n";
                        return true;
                    }
                }
                std::cout << "      No match for predicate\n";
            }
            return false;
        }

        bool check_conditions(const std::vector<actions::condition_t>& conditions, std::map<std::string, std::string>& bindings) {
            for (const auto& cond : conditions) {
                if (!check_condition(cond, bindings)) {
                    return false;
                }
            }
            return true;
        }

        void apply_rule(const actions::rule_t& rule, std::vector<actions::relation_t>& new_relations) {
            std::map<std::string, std::string> bindings;
            std::cout << "      Checking conditions for rule: " << rule.name << "\n";
            if (check_conditions(rule.conditions, bindings)) {
                actions::relation_t new_relation;
                new_relation.relation_name = rule.conclusion.relation_name;
                new_relation.var1 = bindings.count(rule.conclusion.var1) ? bindings[rule.conclusion.var1] : rule.conclusion.var1;
                new_relation.var2 = bindings.count(rule.conclusion.var2) ? bindings[rule.conclusion.var2] : rule.conclusion.var2;
                new_relation.attributes = rule.conclusion.attributes;
                std::cout << "      Rule applied: New relation " << new_relation.relation_name
                          << "(" << new_relation.var1 << ", " << new_relation.var2 << ")";
                if (!new_relation.attributes.empty()) {
                    std::cout << " WITH ";
                    for (size_t i = 0; i < new_relation.attributes.size(); ++i) {
                        std::cout << new_relation.attributes[i].key << "=\"" << new_relation.attributes[i].value << "\"";
                        if (i < new_relation.attributes.size() - 1) std::cout << ", ";
                    }
                }
                std::cout << "\n";
                if (is_transitive_rule(rule)) {
                    std::cout << "      Transitive rule: " << rule.name << "\n";
                }
                new_relations.push_back(new_relation);
            } else {
                std::cout << "      Rule conditions not satisfied\n";
            }
        }

        bool is_transitive_rule(const actions::rule_t& rule) {
            if (rule.conditions.size() < 2) return false;
            std::string first_var, second_var, third_var;
            bool found_first = false;
            for (const auto& cond : rule.conditions) {
                if (std::holds_alternative<actions::relation_t>(cond.value)) {
                    const auto& rel = std::get<actions::relation_t>(cond.value);
                    if (!found_first) {
                        first_var = rel.var1;
                        second_var = rel.var2;
                        found_first = true;
                    } else if (rel.var1 == second_var) {
                        third_var = rel.var2;
                        if (rule.conclusion.var1 == first_var && rule.conclusion.var2 == third_var) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        bool exceeds_max_depth(const actions::rule_t& rule, int max_depth) {
            if (!is_transitive_rule(rule)) return false;
            int relation_count = 0;
            for (const auto& cond : rule.conditions) {
                if (std::holds_alternative<actions::relation_t>(cond.value)) {
                    ++relation_count;
                }
            }
            return relation_count > max_depth;
        }

        bool matches_context(const std::string& rule_context, const std::string& query_context) {
            if (rule_context == query_context || query_context.empty()) return true;
            if (rule_context == "*/*") return true;
            auto split = [](const std::string& ctx) -> std::pair<std::string, std::string> {
                auto pos = ctx.find('/');
                if (pos == std::string::npos) return {ctx, ""};
                return {ctx.substr(0, pos), ctx.substr(pos + 1)};
            };
            auto [rule_type, rule_subtype] = split(rule_context);
            auto [query_type, query_subtype] = split(query_context);
            bool type_match = (rule_type == "*" || query_type == "*" || rule_type == query_type);
            bool subtype_match = (rule_subtype.empty() || rule_subtype == "*" || query_subtype == "*" || rule_subtype == query_subtype);
            return type_match && subtype_match;
        }

        actions::rule_state state_;
        std::vector<actions::relation_t> facts_;
        std::vector<actions::predicate_t> predicates_;
    };
}
