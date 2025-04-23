#include "inference_engine.h"
#include <iostream>
#include <algorithm>

namespace sen {
void InferenceEngine::parse(const std::string& dsl) {
    tao::pegtl::string_input<> input(dsl, "rules");
    try {
        state.reset();
        tao::pegtl::parse<grammar::grammar, actions::action>(input, state);
        std::cout << "Parsed DSL successfully. Contexts: " << state.contexts.size() << "\n";
        for (const auto& ctx : state.contexts) {
            std::cout << "  Context: " << ctx.mime_type << ", Rules: " << ctx.rules.size() << "\n";
            for (const auto& rule : ctx.rules) {
                std::cout << "    Rule: " << rule.name << ", Conditions: " << rule.conditions.size()
                          << ", Conclusion: " << rule.conclusion.relation_name << "\n";
            }
        }
        for (const auto& [alias, rel] : state.aliases) {
            std::cout << "  Alias: " << alias << " -> " << rel << "\n";
        }
    } catch (const tao::pegtl::parse_error& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        std::cerr << "At position: " << e.positions()[0].byte << "\n";
        throw;
    }
}

void InferenceEngine::add_fact(const std::string& relation, const std::string& entity1, const std::string& entity2,
                               const std::vector<actions::attribute_t>& attributes) {
    std::string resolved_relation = resolve_alias(relation);
    facts.push_back({entity1, resolved_relation, entity2, attributes});
    std::cout << "Added fact: " << resolved_relation << "(" << entity1 << ", " << entity2 << ")";
    if (!attributes.empty()) {
        std::cout << " WITH ";
        for (size_t i = 0; i < attributes.size(); ++i) {
            std::cout << attributes[i].key << "=\"" << attributes[i].value << "\"";
            if (i < attributes.size() - 1) std::cout << ", ";
        }
    }
    std::cout << "\n";
}

void InferenceEngine::add_predicate(const std::string& entity, const std::string& key, const std::string& value) {
    predicates.push_back({entity, key, value});
    std::cout << "Added predicate: " << entity << " has " << key << "=\"" << value << "\"\n";
}

std::vector<actions::relation_t> InferenceEngine::infer(const std::string& context, int max_depth,
                                                       int max_iterations) {
    std::vector<actions::relation_t> new_relations;
    std::cout << "Starting inference: context=" << context << ", max_depth=" << max_depth
              << ", iterations=" << max_iterations << "\n";

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        std::cout << "Iteration " << (iteration + 1) << "\n";
        size_t initial_size = new_relations.size();
        for (const auto& ctx : state.contexts) {
            if (matches_context(ctx.mime_type, context)) {
                int match_count = 0;
                std::cout << "  Checking context: " << ctx.mime_type << "\n";
                for (const auto& rule : ctx.rules) {
                    std::cout << "    Applying rule: " << rule.name << "\n";
                    auto matches = apply_rule(rule, max_depth);
                    for (const auto& match : matches) {
                        bool is_duplicate = std::any_of(new_relations.begin(), new_relations.end(), [&](const auto& r) {
                            if (r.var1 != match.var1 || r.var2 != match.var2 || r.relation_name != match.relation_name ||
                                r.attributes.size() != match.attributes.size()) {
                                return false;
                            }
                            std::vector<actions::attribute_t> r_attrs = r.attributes;
                            std::vector<actions::attribute_t> m_attrs = match.attributes;
                            std::sort(r_attrs.begin(), r_attrs.end(),
                                      [](const auto& a, const auto& b) { return a.key < b.key; });
                            std::sort(m_attrs.begin(), m_attrs.end(),
                                      [](const auto& a, const auto& b) { return a.key < b.key; });
                            for (size_t i = 0; i < r_attrs.size(); ++i) {
                                if (r_attrs[i] != m_attrs[i]) {
                                    return false;
                                }
                            }
                            return true;
                        }) || std::any_of(facts.begin(), facts.end(), [&](const auto& f) {
                            if (f.var1 != match.var1 || f.var2 != match.var2 || f.relation_name != match.relation_name ||
                                f.attributes.size() != match.attributes.size()) {
                                return false;
                            }
                            std::vector<actions::attribute_t> f_attrs = f.attributes;
                            std::vector<actions::attribute_t> m_attrs = match.attributes;
                            std::sort(f_attrs.begin(), f_attrs.end(),
                                      [](const auto& a, const auto& b) { return a.key < b.key; });
                            std::sort(m_attrs.begin(), m_attrs.end(),
                                      [](const auto& a, const auto& b) { return a.key < b.key; });
                            for (size_t i = 0; i < f_attrs.size(); ++i) {
                                if (f_attrs[i] != m_attrs[i]) {
                                    return false;
                                }
                            }
                            return true;
                        });
                        if (!is_duplicate) {
                            new_relations.push_back(match);
                            match_count++;
                            std::cout << "        Added relation: " << match.relation_name << "(" << match.var1
                                      << ", " << match.var2 << ")\n";
                        } else {
                            std::cout << "        Skipped duplicate: " << match.relation_name << "(" << match.var1
                                      << ", " << match.var2 << ")\n";
                        }
                    }
                }
                std::cout << "    Matches found: " << match_count << "\n";
            }
        }
        if (new_relations.size() == initial_size) {
            std::cout << "  No new relations in iteration " << (iteration + 1) << ", stopping\n";
            break;
        }
        facts.insert(facts.end(), new_relations.begin() + initial_size, new_relations.end());
    }
    std::cout << "Inference complete. New relations: " << new_relations.size() << "\n";
    for (const auto& rel : new_relations) {
        std::cout << "New relation: " << rel.relation_name << "(" << rel.var1 << ", " << rel.var2 << ")";
        if (!rel.attributes.empty()) {
            std::cout << " WITH ";
            for (size_t i = 0; i < rel.attributes.size(); ++i) {
                std::cout << rel.attributes[i].key << "=\"" << rel.attributes[i].value << "\"";
                if (i < rel.attributes.size() - 1) std::cout << ", ";
            }
        }
        std::cout << "\n";
    }
    return new_relations;
}

bool InferenceEngine::matches_context(const std::string& rule_context, const std::string& query_context) const {
    if (query_context == "*/*" || rule_context == "*/*") return true;
    auto split = [](const std::string& s) {
        auto pos = s.find('/');
        return std::make_pair(s.substr(0, pos), pos == std::string::npos ? "" : s.substr(pos + 1));
    };
    auto [rule_type, rule_subtype] = split(rule_context);
    auto [query_type, query_subtype] = split(query_context);

    return (rule_type == query_type || rule_type == "*" || query_type == "*") &&
           (rule_subtype == query_subtype || rule_subtype == "*" || query_subtype == "*");
}

std::string InferenceEngine::resolve_alias(const std::string& relation) const {
    auto it = state.aliases.find(relation);
    std::string resolved = it != state.aliases.end() ? it->second : relation;
    std::cout << "      Resolving alias: " << relation << " -> " << resolved << "\n";
    return resolved;
}

bool InferenceEngine::matches_condition(const actions::condition_t& condition,
                                       std::map<std::string, std::string>& bindings, int depth) const {
    if (depth <= 0) return false;

    return std::visit(
        [&](const auto& cond) {
            using T = std::decay_t<decltype(cond)>;
            if constexpr (std::is_same_v<T, actions::relation_t>) {
                std::cout << "      Checking relation: " << cond.var1 << " ~" << cond.relation_name << " " << cond.var2 << "\n";
                std::string resolved_relation = resolve_alias(cond.relation_name);
                for (const auto& fact : facts) {
                    if (fact.relation_name == resolved_relation) {
                        std::map<std::string, std::string> new_bindings = bindings;
                        bool vars_unbound = new_bindings.find(cond.var1) == new_bindings.end() &&
                                           new_bindings.find(cond.var2) == new_bindings.end();
                        bool vars_match = new_bindings.count(cond.var1) && new_bindings[cond.var1] == fact.var1 &&
                                          new_bindings.count(cond.var2) && new_bindings[cond.var2] == fact.var2;
                        if (vars_unbound || vars_match) {
                            new_bindings[cond.var1] = fact.var1;
                            new_bindings[cond.var2] = fact.var2;
                            bool attributes_match = true;
                            for (const auto& attr : cond.attributes) {
                                std::cout << "        Checking attribute: " << attr.key << "=" << attr.value << "\n";
                                auto it = std::find_if(fact.attributes.begin(), fact.attributes.end(),
                                                       [&](const auto& fa) { return fa == attr; });
                                if (it == fact.attributes.end()) {
                                    std::cout << "        Attribute mismatch: " << attr.key << "=" << attr.value << "\n";
                                    attributes_match = false;
                                    break;
                                }
                            }
                            if (attributes_match) {
                                std::cout << "        Match found: " << fact.var1 << " ~" << resolved_relation
                                          << " " << fact.var2 << "\n";
                                std::cout << "        New bindings: " << cond.var1 << "=" << fact.var1 << ", "
                                          << cond.var2 << "=" << fact.var2 << "\n";
                                bindings = new_bindings;
                                return true;
                            }
                        }
                    }
                }
                std::cout << "        No match for relation: " << cond.var1 << " ~" << resolved_relation << " " << cond.var2 << "\n";
                return false;
            } else if constexpr (std::is_same_v<T, actions::predicate_t>) {
                std::cout << "      Checking predicate: " << cond.var << " has " << cond.key << "=\"" << cond.value << "\"\n";
                auto it = bindings.find(cond.var);
                if (it == bindings.end()) {
                    std::cout << "        No binding for " << cond.var << "\n";
                    return false;
                }
                const std::string& entity = it->second;
                for (const auto& pred : predicates) {
                    if (pred.var == entity && pred.key == cond.key && pred.value == cond.value) {
                        std::cout << "        Match found: " << entity << " has " << pred.key << "=\"" << pred.value << "\"\n";
                        return true;
                    }
                }
                std::cout << "        No predicate match for " << entity << " has " << cond.key << "=\"" << cond.value << "\"\n";
                return false;
            }
            return false;
        },
        condition.value);
}

std::vector<actions::relation_t> InferenceEngine::apply_rule(const actions::rule_t& rule, int max_depth) const {
    std::vector<actions::relation_t> new_relations;
    std::cout << "      Checking conditions for rule: " << rule.name << "\n";
    if (rule.conditions.empty()) return new_relations;

    std::vector<std::map<std::string, std::string>> all_bindings;
    auto check_conditions = [&](const auto& self, size_t cond_idx, std::map<std::string, std::string>& bindings,
                               int depth) -> void {
        if (cond_idx >= rule.conditions.size()) {
            all_bindings.push_back(bindings);
            return;
        }
        if (depth <= 0) return;

        std::map<std::string, std::string> new_bindings = bindings;
        if (matches_condition(rule.conditions[cond_idx], new_bindings, depth)) {
            std::cout << "        Condition " << cond_idx << " matched with bindings: ";
            for (const auto& [var, val] : new_bindings) {
                std::cout << var << "=" << val << " ";
            }
            std::cout << "\n";
            self(self, cond_idx + 1, new_bindings, depth - 1);
        }
        // Only iterate facts for multi-condition rules to avoid duplicate bindings
        if (rule.conditions.size() > 1 && std::holds_alternative<actions::relation_t>(rule.conditions[cond_idx].value)) {
            const auto& cond = std::get<actions::relation_t>(rule.conditions[cond_idx].value);
            std::string resolved_relation = resolve_alias(cond.relation_name);
            for (const auto& fact : facts) {
                if (fact.relation_name == resolved_relation) {
                    new_bindings = bindings;
                    if ((new_bindings.count(cond.var1) == 0 || new_bindings[cond.var1] == fact.var1) &&
                        (new_bindings.count(cond.var2) == 0 || new_bindings[cond.var2] == fact.var2)) {
                        new_bindings[cond.var1] = fact.var1;
                        new_bindings[cond.var2] = fact.var2;
                        bool attributes_match = true;
                        for (const auto& attr : cond.attributes) {
                            auto it = std::find_if(fact.attributes.begin(), fact.attributes.end(),
                                                   [&](const auto& fa) { return fa == attr; });
                            if (it == fact.attributes.end()) {
                                attributes_match = false;
                                break;
                            }
                        }
                        if (attributes_match && matches_condition(rule.conditions[cond_idx], new_bindings, depth)) {
                            self(self, cond_idx + 1, new_bindings, depth - 1);
                        }
                    }
                }
            }
        }
    };

    std::map<std::string, std::string> initial_bindings;
    check_conditions(check_conditions, 0, initial_bindings, max_depth);

    std::cout << "      Total bindings sets: " << all_bindings.size() << "\n";
    for (const auto& bindings : all_bindings) {
        std::cout << "      Bindings: ";
        for (const auto& [var, val] : bindings) {
            std::cout << var << "=" << val << " ";
        }
        std::cout << "\n";
        const auto& conclusion = rule.conclusion;
        actions::relation_t new_relation;
        new_relation.var1 = bindings.count(conclusion.var1) ? bindings.at(conclusion.var1) : "";
        new_relation.var2 = bindings.count(conclusion.var2) ? bindings.at(conclusion.var2) : "";
        new_relation.relation_name = resolve_alias(conclusion.relation_name);
        new_relation.attributes = conclusion.attributes;
        if (!new_relation.var1.empty() && !new_relation.var2.empty()) {
            std::cout << "      Rule applied: New relation " << new_relation.relation_name << "(" << new_relation.var1
                      << ", " << new_relation.var2 << ")";
            if (!new_relation.attributes.empty()) {
                std::cout << " WITH ";
                for (size_t i = 0; i < new_relation.attributes.size(); ++i) {
                    std::cout << new_relation.attributes[i].key << "=\"" << new_relation.attributes[i].value << "\"";
                    if (i < new_relation.attributes.size() - 1) std::cout << ", ";
                }
            }
            std::cout << "\n";
            new_relations.push_back(new_relation);
        } else {
            std::cout << "      Skipped relation due to empty vars: " << new_relation.relation_name << "\n";
        }
    }

    return new_relations;
}
} // namespace sen