#pragma once

#include "sen_grammar.h"
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace sen {
class InferenceEngine {
public:
    void parse(const std::string& dsl);
    void add_fact(const std::string& relation, const std::string& entity1, const std::string& entity2,
                  const std::vector<sen::actions::attribute_t>& attributes = {});
    void add_predicate(const std::string& entity, const std::string& key, const std::string& value);
    std::vector<sen::actions::relation_t> infer(const std::string& context = "*/*", int max_depth = 2,
                                                   int max_iterations = 2);

private:
    using fact_t = sen::actions::relation_t;
    using predicate_t = sen::actions::predicate_t;

    sen::actions::rule_state state;
    std::vector<fact_t> facts;
    std::vector<predicate_t> predicates;

    bool matches_context(const std::string& rule_context, const std::string& query_context) const;
    std::string resolve_alias(const std::string& relation) const;
    bool matches_condition(const sen::actions::condition_t& condition,
                          const std::map<std::string, std::string>& bindings, int depth) const;
    std::vector<fact_t> apply_rule(const sen::actions::rule_t& rule, int max_depth) const;
};
} // namespace sen