#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include "sen_grammar.h"

namespace sen {
    // Reused from parser
    using Attribute = sen::actions::attribute_t; // {key, value}
    using Relation = sen::actions::relation_t;   // {relation_name, var1, var2, attributes}
    using Predicate = sen::actions::predicate_t; // {var, key, value}
    using Condition = sen::actions::condition_t; // std::variant<Relation, Predicate>
    using Rule = sen::actions::rule_t;          // {name, conditions, conclusion}
    using Context = sen::actions::context_t;    // {mime_type, rules}

    class InferenceEngine {
    public:
        // Parse DSL
        void parse(const std::string& input) {
            state_.reset();
            tao::pegtl::memory_input in(input, "rules");
            tao::pegtl::parse<grammar::grammar, actions::action>(in, state_);
        }

        // Add facts and predicates
        void add_fact(const Relation& fact) { facts_.push_back(fact); }
        void add_predicate(const Predicate& pred) { predicates_.push_back(pred); }

        // Infer new relations with optional context, max_depth, and iterations
        std::vector<Relation> infer(const std::optional<std::string>& context = std::nullopt,
                                   int max_depth = 2, int iterations = 1) {
            std::vector<Relation> all_new_relations;
            for (int iter = 0; iter < iterations; ++iter) {
                std::vector<Relation> new_relations;
                for (const auto& ctx : state_.contexts) {
                    // Skip if context specified and doesn't match
                    if (context && ctx.mime_type != *context) continue;
                    for (const auto& rule : ctx.rules) {
                        // Skip transitive rules if they exceed max_depth
                        if (is_transitive_rule(rule) && exceeds_max_depth(rule, max_depth)) {
                            continue;
                        }
                        apply_rule(rule, new_relations);
                    }
                }
                if (new_relations.empty()) break; // No new relations, stop early
                all_new_relations.insert(all_new_relations.end(), new_relations.begin(), new_relations.end());
                facts_.insert(facts_.end(), new_relations.begin(), new_relations.end());
            }
            return all_new_relations;
        }

    private:
        // Check if a condition matches facts/predicates, updating bindings
        bool check_condition(const Condition& cond, std::map<std::string, std::string>& bindings) {
            if (std::holds_alternative<Relation>(cond.value)) {
                const auto& query = std::get<Relation>(cond.value);
                std::string resolved_name = state_.aliases.count(query.relation_name) ? state_.aliases[query.relation_name] : query.relation_name;
                for (const auto& fact : facts_) {
                    std::string fact_name = state_.aliases.count(fact.relation_name) ? state_.aliases[fact.relation_name] : fact.relation_name;
                    if (fact_name == resolved_name && fact.var2.size() == query.var2.size()) {
                        std::map<std::string, std::string> temp_bindings = bindings;
                        bool args_match = true;
                        if (temp_bindings.count(query.var1)) {
                            if (temp_bindings[query.var1] != fact.var1) args_match = false;
                        } else {
                            temp_bindings[query.var1] = fact.var1;
                        }
                        if (temp_bindings.count(query.var2)) {
                            if (temp_bindings[query.var2] != fact.var2) args_match = false;
                        } else {
                            temp_bindings[query.var2] = fact.var2;
                        }
                        if (args_match) {
                            bindings = std::move(temp_bindings);
                            return true;
                        }
                    }
                }
            } else if (std::holds_alternative<Predicate>(cond.value)) {
                const auto& query = std::get<Predicate>(cond.value);
                for (const auto& pred : predicates_) {
                    if (pred.var == query.var && pred.key == query.key && pred.value == query.value) {
                        return true;
                    }
                }
            }
            return false;
        }

        // Check all conditions in a rule
        bool check_conditions(const std::vector<Condition>& conditions, std::map<std::string, std::string>& bindings) {
            for (const auto& cond : conditions) {
                if (!check_condition(cond, bindings)) {
                    return false;
                }
            }
            return true;
        }

        // Apply a rule to generate new relations
        void apply_rule(const Rule& rule, std::vector<Relation>& new_relations) {
            std::map<std::string, std::string> bindings;
            if (check_conditions(rule.conditions, bindings)) {
                Relation new_relation;
                new_relation.relation_name = rule.conclusion.relation_name;
                new_relation.var1 = bindings.count(rule.conclusion.var1) ? bindings[rule.conclusion.var1] : rule.conclusion.var1;
                new_relation.var2 = bindings.count(rule.conclusion.var2) ? bindings[rule.conclusion.var2] : rule.conclusion.var2;
                new_relation.attributes = rule.conclusion.attributes;
                if (is_transitive_rule(rule)) {
                    std::cout << "Applying transitive rule: " << rule.name << "\n";
                }
                new_relations.push_back(new_relation);
            }
        }

        // Transitive rule detection (ignoring relation names)
        bool is_transitive_rule(const Rule& rule) {
            if (rule.conditions.size() < 2) return false;
            std::string first_var, second_var, third_var;
            bool found_first = false;
            for (const auto& cond : rule.conditions) {
                if (std::holds_alternative<Relation>(cond.value)) {
                    const auto& rel = std::get<Relation>(cond.value);
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

        // Check if a transitive rule exceeds max_depth
        bool exceeds_max_depth(const Rule& rule, int max_depth) {
            if (!is_transitive_rule(rule)) return false;
            // For transitive rules (A->B->C), depth is 2
            // Count relation hops in conditions
            int relation_count = 0;
            for (const auto& cond : rule.conditions) {
                if (std::holds_alternative<Relation>(cond.value)) {
                    ++relation_count;
                }
            }
            // Depth is number of relation hops (e.g., A->B->C has 2 hops)
            return relation_count > max_depth;
        }

        actions::rule_state state_; // Parsed rules, contexts, aliases
        std::vector<Relation> facts_; // Known relations
        std::vector<Predicate> predicates_; // Known predicates
    };
}
