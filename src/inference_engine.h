// FILE: inference_engine.h
#ifndef INFERENCE_ENGINE_H
#define INFERENCE_ENGINE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <tao/pegtl.hpp>
#include <stdexcept>
#include <sstream>
#include <iostream>

using namespace tao::pegtl;

struct Entity {
    std::string id;  // SEN:ID
    std::unordered_map<std::string, std::string> properties;
};

struct Relation {
    std::string from;  // SEN:ID of source file
    std::string to;    // SEN:ID of target file
    std::string type;  // Relation type (e.g., relation/book-quote)
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

    void addRelation(const std::string& from, const std::string& to, const std::string& type) {
        relations.push_back({from, to, type});
    }
};

// Grammar rules
struct Identifier : plus<sor<alpha, one<'/'>>> {};
struct String : seq<one<'"'>, plus<not_at<one<'"'>>, any>, one<'"'>> {};
struct RelationName : seq<string<'r', 'e', 'l', 'a', 't', 'i', 'o', 'n', '/'>, plus<alnum, one<'-'>>> {};
struct UseDecl : seq<string<'u', 's', 'e'>, space, RelationName, space, string<'A', 'S'>, space, Identifier> {};
struct HasProperty : seq<Identifier, string<' ', 'h', 'a', 's'>, space, String, space, Identifier> {};
struct TildeRelation : seq<Identifier, space, one<'~'>, Identifier, space, Identifier> {};
struct Condition : sor<HasProperty, TildeRelation> {};
struct IfClause : seq<string<'i', 'f'>, space, one<'('>, Condition, opt<seq<string<' ', 'a', 'n', 'd'>, space, Condition>>, one<')'>> {};
struct ThenClause : seq<string<'t', 'h', 'e', 'n'>, space, string<'r', 'e', 'l', 'a', 't', 'e'>, one<'('>, Identifier, one<','>, space, Identifier, one<','>, space, String, one<')'>> {};
struct Rule : seq<string<'r', 'u', 'l', 'e'>, space, Identifier, space, one<'{'>, space, IfClause, space, ThenClause, space, one<'}'>> {};
struct ContextDef : seq<string<'c', 'o', 'n', 't', 'e', 'x', 't'>, space, Identifier, space, one<'{'>, plus<Rule>, one<'}'>> {};
struct RuleOrContext : sor<ContextDef, Rule> {};
struct Grammar : seq<star<UseDecl, space>, plus<RuleOrContext>, eof> {};

// Actions
struct RuleDef {
    std::string name;
    std::vector<std::string> conditions;
    std::string from, to, relation;
};

struct ContextDefStruct {
    std::string mimeType;
    std::vector<RuleDef> rules;
};

struct State {
    std::vector<ContextDefStruct> contexts;
    std::unordered_map<std::string, std::string> aliases;
    std::string currentBlock;
};

template<typename Rule> struct Action : nothing<Rule> {};

template<> struct Action<UseDecl> {
    template<typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t asPos = s.find(" AS ");
        state.aliases[s.substr(asPos + 4)] = s.substr(4, asPos - 4);
        state.currentBlock = "use " + s.substr(4, asPos - 4);
    }
};

template<> struct Action<Rule> {
    template< typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        RuleDef rule;
        rule.name = in.string().substr(5, in.string().find('{') - 6);
        if (state.contexts.empty() || !state.contexts.back().mimeType.empty()) {
            state.contexts.push_back({"", {}});
        }
        state.contexts.back().rules.push_back(rule);
        state.currentBlock = "rule " + rule.name;
    }
};

template<> struct Action<HasProperty> {
    template< typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        state.contexts.back().rules.back().conditions.push_back(in.string());
    }
};

template<> struct Action<TildeRelation> {
    template< typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        state.contexts.back().rules.back().conditions.push_back(in.string());
    }
};

template<> struct Action<ThenClause> {
    template< typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t firstComma = s.find(',');
        size_t secondComma = s.find(',', firstComma + 1);
        auto& rule = state.contexts.back().rules.back();
        rule.from = s.substr(11, firstComma - 11);
        rule.to = s.substr(firstComma + 2, secondComma - firstComma - 2);
        rule.relation = s.substr(secondComma + 2, s.size() - secondComma - 3);
    }
};

template<> struct Action<ContextDef> {
    template< typename ActionInput>

    static void apply(const ActionInput& in, State& state) {
        ContextDefStruct context;
        context.mimeType = in.string().substr(8, in.string().find('{') - 9);
        state.contexts.push_back(context);
    }
};

class InferenceEngine {
    State state;
    KnowledgeBase& kb;
    int maxDepth = 2;

public:
    InferenceEngine(KnowledgeBase& knowledge, int depth = 2) : kb(knowledge), maxDepth(depth) {
        if (maxDepth > 2) maxDepth = 2;
    }

    void loadDSL(const std::string& dsl) {
        try {
            parse<Grammar, Action>(memory_input(dsl, "DSL"), state);
/*
            if (state.contexts.empty() && state.aliases.empty()) {
                throw std::runtime_error("No valid contexts or aliases parsed from DSL");
            }
            */
        } catch (const parse_error& e) {
            throw std::runtime_error(std::string("DSL parsing failed: ") + e.what());
        }
    }

    void loadFromSource(const std::string& sourceId) {
        std::unordered_set<std::string> loadedIds;
        loadEntityAndRelations(sourceId, 0, loadedIds);
    }

    void infer(const std::string& sourceMimeType) {
        for (const auto& context : state.contexts) {
            if (context.mimeType.empty() || context.mimeType == sourceMimeType) {
                for (const auto& rule : context.rules) {
                    applyRule(rule);
                }
            }
        }
    }

private:
    void loadEntityAndRelations(const std::string& id, int depth, std::unordered_set<std::string>& loadedIds) {
        if (depth > maxDepth || loadedIds.count(id)) return;

        loadEntity(id);
        loadedIds.insert(id);
        auto relations = getRelationsForId(id);

        for (const auto& rel : relations) {
            kb.addRelation(rel.from, rel.to, rel.type);
            loadEntityAndRelations(rel.to, depth + 1, loadedIds);
        }
    }

    void loadEntity(const std::string& id) {
        std::unordered_map<std::string, std::string> props = queryEntityAttributes(id);
        kb.addEntity(id, props);
    }

    std::vector<Relation> getRelationsForId(const std::string& id) {
        std::vector<std::string> toIds = querySENto(id);
        std::vector<Relation> relations;

        for (const auto& toId : toIds) {
            std::string relType = queryRelationType(id, toId);
            relations.push_back({id, toId, relType});
        }

        return relations;
    }

    // Haiku stubs (replace with SEN core implementations)
    std::unordered_map<std::string, std::string> queryEntityAttributes(const std::string& senId) {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dummyProps = {
            {"id1", {{"name", "AuthorA"}, {"mime", "text/book"}}},
            {"id2", {{"name", "AuthorB"}, {"mime", "text/book"}}},
            {"id3", {{"name", "AuthorC"}, {"mime", "application/person"}}}
        };
        return dummyProps[senId];
    }

    std::vector<std::string> querySENto(const std::string& senId) {
        static std::unordered_map<std::string, std::vector<std::string>> dummySENto = {
            {"id1", {"id2"}},
            {"id2", {"id3"}},
            {"id3", {}}
        };
        return dummySENto[senId];
    }

    std::string queryRelationType(const std::string& fromId, const std::string& toId) {
        static std::unordered_map<std::string, std::string> dummyTypes = {
            {"id1id2", "relation/book-quote"},
            {"id2id3", "relation/family-link"}
        };
        return dummyTypes[fromId + toId];
    }

    void applyRule(const RuleDef& rule) {
        for (const auto& e1 : kb.entities) {
            for (const auto& e2 : kb.entities) {
               // for (const auto& e3 : kb.entities) {
                    bool conditionsMet = true;
                    std::unordered_map<std::string, std::string> varMap;

                    for (const auto& cond : rule.conditions) {
                        if (cond.find("~") != std::string::npos) {
                            std::string alias = extractAlias(cond);
                            std::string relType = state.aliases.at(alias);
                            std::string var1 = cond.substr(0, cond.find(' '));
                            std::string var2 = cond.substr(cond.rfind(' ') + 1);

                            varMap[var1] = e1.id;
                            varMap[var2] = e2.id;

                            bool found = false;
                            for (const auto& rel : kb.relations) {
                                if (rel.type == relType && rel.from == e1.id && rel.to == e2.id) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) conditionsMet = false;
                        } else if (cond.find("has \"") != std::string::npos) {
                            std::string prop = extractQuoted(cond);
                            std::string var1 = cond.substr(0, cond.find(' '));
                            std::string var2 = cond.substr(cond.rfind(' ') + 1);

                            varMap[var1] = e1.id;
                            varMap[var2] = e2.id;
                            auto it = e1.properties.find(prop);

                            if (it == e1.properties.end() || (e2.id != it->second && var2 == it->second)) {
                                conditionsMet = false;
                            }
                        }
                    }
                    if (conditionsMet) {
                        kb.addRelation(varMap[rule.from], varMap[rule.to], rule.relation);
                    }
               // }
            }
        }
    }

    std::string extractAlias(const std::string& cond) {
        size_t tildePos = cond.find('~');
        size_t spacePos = cond.find(' ', tildePos);
        return cond.substr(tildePos + 1, spacePos - tildePos - 1);
    }

    std::string extractQuoted(const std::string& cond) {
        size_t start = cond.find('"') + 1;
        size_t end = cond.find('"', start);
        return cond.substr(start, end - start);
    }
};

#endif

