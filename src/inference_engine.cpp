#include "inference_engine.h"
#include <iostream>

namespace sen {

std::string unquote(const std::string s) {
    std::string result;

    std::istringstream strstr(s);
    strstr >> std::quoted(result);

    return result;
}

void InferenceEngine::parse(const std::string& dsl) {
    tao::pegtl::string_input<> input(dsl, "rules");
    try {
        state.reset();
        tao::pegtl::parse<grammar::grammar, sen::actions::action>(input, state);
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
       // std::cerr << "Near: " << input.substr(e.positions()[0].byte, 20) << "\n";
        throw;
    }
}

void InferenceEngine::add_fact(const std::string& relation, const std::string& entity1, const std::string& entity2,
                               const std::vector<sen::actions::attribute_t>& attributes) {

    std::string resolved_relation = resolve_alias(relation);
    facts.push_back({entity1, resolved_relation, entity2, attributes});

    std::cout << "Added fact: " << relation << "(" << entity1 << ", " << entity2 << ")";
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

std::vector<sen::actions::relation_t> InferenceEngine::infer(const std::string& context, int max_depth,
                                                                int max_iterations) {
    std::vector<sen::actions::relation_t> new_relations;
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
                    match_count += matches.size();
                    new_relations.insert(new_relations.end(), matches.begin(), matches.end());
                }
                std::cout << "    Matches found: " << match_count << "\n";
            }
        }
        if (new_relations.size() == initial_size) {
            std::cout << "  No new relations in iteration " << (iteration + 1) << ", stopping\n";
            break;
        }
    }
    std::cout << "Inference complete. New relations: " << new_relations.size() << "\n";
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

bool InferenceEngine::matches_condition(const sen::actions::condition_t& condition,
                                       const std::map<std::string, std::string>& bindings, int depth) const {
    if (depth <= 0) return false;

    return std::visit(
        [&](const auto& cond) {
            using T = std::decay_t<decltype(cond)>;
            if constexpr (std::is_same_v<T, sen::actions::relation_t>) {
                std::cout << "      Checking relation: " << cond.var1 << " ~" << cond.relation_name << " " << cond.var2 << "\n";
                std::string resolved_relation = resolve_alias(cond.relation_name);
                for (const auto& fact : facts) {
                    if (fact.relation_name == resolved_relation) {
                        std::map<std::string, std::string> new_bindings = bindings;
                        if (new_bindings.emplace(cond.var1, fact.var1).second &&
                            new_bindings.emplace(cond.var2, fact.var2).second) {
                            bool attributes_match = true;
                            for (const auto& attr : cond.attributes) {
                                auto it = std::find_if(fact.attributes.begin(), fact.attributes.end(),
                                                       [&](const auto& fa) { return fa.key == attr.key && fa.value == attr.value; });
                                if (it == fact.attributes.end()) {
                                    attributes_match = false;
                                    break;
                                }
                            }
                            if (attributes_match) {
                                std::cout << "        Match found: " << fact.var1 << " ~" << resolved_relation
                                          << " " << fact.var2 << "\n";
                                return true;
                            }
                        }
                    }
                }
                return false;
            } else if constexpr (std::is_same_v<T, sen::actions::predicate_t>) {
                std::cout << "      Checking predicate: " << cond.var << " has " << cond.key << "=\"" << cond.value << "\"\n";
                auto it = bindings.find(cond.var);
                if (it == bindings.end()) return false;
                const std::string& entity = it->second;
                for (const auto& pred : predicates) {
                    if (pred.var == entity && pred.key == cond.key && pred.value == cond.value) {
                        std::cout << "        Match found: " << entity << " has " << pred.key << "=\"" << pred.value << "\"\n";
                        return true;
                    }
                }
                return false;
            }
            return false;
        },
        condition.value);
}

std::vector<sen::actions::relation_t> InferenceEngine::apply_rule(const sen::actions::rule_t& rule,
                                                                  int max_depth) const {
    std::vector<sen::actions::relation_t> new_relations;
    std::cout << "      Checking conditions for rule: " << rule.name << "\n";

    if (rule.conditions.empty()) {
        std::cout << "no conditions defined for rule " << rule.name << ", aborting.\n";
        return new_relations;
    }

    std::map<std::string, std::string> bindings;

    auto check_conditions = [&](const auto& self, size_t cond_idx, auto& bindings, int depth) -> bool {
        if (cond_idx >= rule.conditions.size()) return true;
        if (depth <= 0) return false;

        return matches_condition(rule.conditions[cond_idx], bindings, depth) &&
               self(self, cond_idx + 1, bindings, depth - 1);
    };

    std::cout << "      Bindings: ";
    for (const auto& [var, val] : bindings) {
        std::cout << var << "=" << val << " ";
    }
    std::cout << "\n";

    if (check_conditions(check_conditions, 0, bindings, max_depth)) {
        const auto& conclusion = rule.conclusion;
        sen::actions::relation_t new_relation;
        new_relation.var1 = bindings[conclusion.var1];
        new_relation.var2 = bindings[conclusion.var2];
        new_relation.relation_name = resolve_alias(conclusion.relation_name);
        new_relation.attributes = conclusion.attributes;
        std::cout << "      Rule applied: New relation " << new_relation.relation_name << "(" << new_relation.var1
                  << ", " << new_relation.var2 << ") WITH ";
        for (const auto& attr : new_relation.attributes) {
            std::cout << attr.key << "=\"" << attr.value << "\"";
            if (&attr != &new_relation.attributes.back()) std::cout << ", ";
        }
        std::cout << "\n";
        new_relations.push_back(new_relation);
    }

    return new_relations;
}
} // namespace sen