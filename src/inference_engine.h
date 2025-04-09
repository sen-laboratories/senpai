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
                     const std::unordered_map<std::string, std::string>& props = {}) {
        relations.push_back({from, to, type, props});
    }
};

// Grammar rules accepting liberal whitespace use
// A C++ style one-line comment. two consecutive slashes followed by anything up to the end of line or end of file.
// line_comment <- '/' '/' .*
struct line_comment :
    seq< two< '/' >, until<eolf> >
    { };

// A single space or a line_comment.
// ws <- space / line_comment
struct ws :
    sor< space/*, line_comment*/ >
    { };

// Zero or more spaces or line_comments
// wss <- ws*
struct wss :  star< ws > { };

// At least one space or line_comment.
// wsp <- ws+
struct wsp : plus< ws > { };

struct wsb : plus<sor<wsp, eol>> {};

struct Special : one<'/','-', '_','.'> {};
struct Identifier : plus<sor<alnum, Special> > {}; // could be a MIME type later
struct String : seq<one<'"'>, plus<not_at<one<'"'>>, any>, one<'"'>> {};
struct TypeName : Identifier {};    // todo: check for valid MIME type
struct HasProperty : seq<Identifier, wsp, string<'h', 'a', 's'>, wsp, String, wsp, Identifier> {};
struct TildeRelation : seq<Identifier, wsp, one<'~'>, Identifier, wsp, Identifier> {};
struct PropertyCheck : seq<Identifier, one<'='>, String> {};
struct Condition : sor<HasProperty, TildeRelation> {};
struct AndClause : seq<plus<ws>, string<'A', 'N', 'D'>, wsp, sor<Condition, PropertyCheck>> {};
struct IfClause : seq<string<'i', 'f'>, plus<ws>, one<'('>, wss, Condition, star<AndClause>, wss, one<')'>> {};
struct PropertyPair : seq<Identifier, one<'='>, String> {};
struct WithClause : seq<plus<ws>, string<'W', 'I', 'T', 'H'>, plus<ws>, PropertyPair, star<seq<one<','>, plus<ws>, PropertyPair>>> {};
struct ThenClause : seq<string<'t', 'h', 'e', 'n'>, wsp, string<'r', 'e', 'l', 'a', 't', 'e'>, wss, one<'('>,
                       Identifier, star<ws>, one<','>, wss, Identifier, wss, one<','>, wss, String, wss, one<')'>,
                       opt<WithClause>> {};

struct Rule : seq<string<'r', 'u', 'l', 'e'>, wsp, Identifier, wsp, one<'{'>, wsb, IfClause, wsb, ThenClause, wsb, one<'}'>> {};
struct ContextDef : seq<string<'c', 'o', 'n', 't', 'e', 'x', 't'>, wsp, Identifier, wsp, one<'{'>, wsb, plus<Rule>, wsb, one<'}'>> {};
struct RuleOrContext : sor<ContextDef, Rule> {};
struct UseDecl : seq<string<'u', 's', 'e'>, wsp, TypeName, wsp, string<'A', 'S'>, wsp, Identifier, eol> {};

struct Statement : sor< UseDecl, RuleOrContext > { };
struct StatementList : plus<Statement, wsb >  { };
struct Grammar : seq< wsb, StatementList, eof > { };

// Actions
struct RuleDef {
    std::string name;
    std::vector<std::string> conditions;
    std::string from, to, relation;
    std::unordered_map<std::string, std::string> relProperties;
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
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        RuleDef rule;
        rule.name = in.string().substr(5, in.string().find('{') - 6);
        std::cout << "parsing rule " << rule.name << std::endl;

        if (state.contexts.empty() || !state.contexts.back().mimeType.empty()) {
            state.contexts.push_back({"", {}});
        }
        state.contexts.back().rules.push_back(rule);
        state.currentBlock = "rule " + rule.name;
    }
};

template<> struct Action<HasProperty> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        state.contexts.back().rules.back().conditions.push_back(in.string());
    }
};

template<> struct Action<TildeRelation> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        state.contexts.back().rules.back().conditions.push_back(in.string());
    }
};

template<> struct Action<PropertyCheck> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        state.contexts.back().rules.back().conditions.push_back(in.string());
    }
};

template<> struct Action<ThenClause> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t firstComma = s.find(',');
        size_t secondComma = s.find(',', firstComma + 1);
        size_t relEnd = s.find(')', secondComma);
        auto& rule = state.contexts.back().rules.back();
        rule.from = s.substr(11, firstComma - 11);
        rule.to = s.substr(firstComma + 2, secondComma - firstComma - 2);
        rule.relation = s.substr(secondComma + 2, relEnd - secondComma - 2);

        // Parse optional WITH clause with commas
        if (s.find(" WITH ") != std::string::npos) {
            std::string withPart = s.substr(s.find(" WITH ") + 6);
            size_t pos = 0;
            while (pos < withPart.length()) {
                size_t eqPos = withPart.find('=', pos);
                if (eqPos == std::string::npos) break;
                std::string key = withPart.substr(pos, eqPos - pos);
                size_t quoteStart = eqPos + 2;  // Skip ="
                size_t quoteEnd = withPart.find('"', quoteStart);
                std::string value = withPart.substr(quoteStart, quoteEnd - quoteStart);
                rule.relProperties[key] = value;
                pos = withPart.find(',', quoteEnd);
                if (pos == std::string::npos) break;
                pos += 2;  // Skip comma and space
            }
        }
    }
};

template<> struct Action<ContextDef> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        ContextDefStruct context;
        context.mimeType = in.string().substr(8, in.string().find('{') - 9);
        std::cout << "parsing context " << context.mimeType << std::endl;

        state.contexts.emplace_back(context);   // todo: was push_back
        state.currentBlock = "context " + context.mimeType;
    }
};

class InferenceEngine {
    State state;
    KnowledgeBase& kb;
    int maxDepth = 2;

public:
    InferenceEngine(KnowledgeBase& knowledge, int depth = 2) : kb(knowledge), maxDepth(depth) {
        maxDepth = depth;
        kb = knowledge;
    }

    void loadDSL(const std::string& dsl) {
        try {
            size_t problems = analyze<Grammar>();
            if (problems > 0) {
                std::cout << "grammar analysis found " << problems << " problems, aborting." << std::endl;
                exit(1);
            }

            //complete_trace<Grammar>(memory_input(dsl, "DSL"), state);
            parse<Grammar, Action>(memory_input(dsl, "DSL"), state);
            std::cout << "Parsed " << state.contexts.size() << " contexts\n";

            for (const auto& ctx : state.contexts) {
                std::cout << "Context: " << ctx.mimeType << ", Rules: " << ctx.rules.size() << "\n";
            }
        } catch (const parse_error& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
            throw;
        }
    }

    void loadFromSource(const std::string& sourceId) {
        std::unordered_set<std::string> loadedIds;
        loadEntityAndRelations(sourceId, 0, loadedIds);
        std::cout << "Loaded " << kb.relations.size() << " relations\n";
    }

    void infer(const std::string& sourceMimeType) {
        std::cout << "infer from type " << sourceMimeType.c_str() << std::endl;
        std::cout << "contexts:" << std::endl;
        for (const auto& context : state.contexts) {
            std::cout << context.mimeType.c_str() << std::endl;
        }

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
        if (depth > maxDepth) {
            std::cout << "reached max depth of " << maxDepth << std::endl;
            return;
        }
        loadEntity(id);
        loadedIds.insert(id);
        auto relations = getRelationsForId(id);

        std::cout << "adding " << relations.size() << " relations for id " << id << " at depth " << depth << std::endl;

        for (const auto& rel : relations) {
            kb.addRelation(rel.from, rel.to, rel.type, rel.properties);
            loadEntityAndRelations(rel.to, ++depth, loadedIds);
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
            auto props = queryRelationProperties(id, toId);
            relations.push_back({id, toId, relType, props});
        }

        return relations;
    }

    // Haiku stubs
    std::unordered_map<std::string, std::string> queryEntityAttributes(const std::string& senId) {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dummyProps = {
            {"id1", {{"name", "AuthorA"}, {"mime", "text/book"}}},
            {"id2", {{"name", "AuthorB"}, {"mime", "text/book"}}},
            {"id3", {{"name", "Alice"}, {"mime", "application/person"}}},
            {"id4", {{"name", "Bob"}, {"mime", "application/person"}}},
            {"id5", {{"name", "Charlie"}, {"mime", "application/person"}}}
        };
        return dummyProps[senId];
    }

    std::vector<std::string> querySENto(const std::string& senId) {
        static std::unordered_map<std::string, std::vector<std::string>> dummySENto = {
            {"id1", {"id2"}},
            {"id2", {}},
            {"id3", {"id4"}},
            {"id4", {"id5"}},
            {"id5", {}}
        };
        return dummySENto[senId];
    }

    std::string queryRelationType(const std::string& fromId, const std::string& toId) {
        static std::unordered_map<std::string, std::string> dummyTypes = {
            {"id1id2", "relation/book-quote"},
            {"id3id4", "relation/family-link"},
            {"id4id5", "relation/family-link"}
        };
        return dummyTypes[fromId + toId];
    }

    std::unordered_map<std::string, std::string> queryRelationProperties(const std::string& fromId, const std::string& toId) {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dummyProps = {
            {"id3id4", {{"role", "parent-of"}}},
            {"id4id5", {{"role", "parent-of"}}}
        };
        return dummyProps[fromId + toId];
    }

    void applyRule(const RuleDef& rule) {
        std::cout << "Applying rule: " << rule.name << "\n";

        for (const auto& e1 : kb.entities) {
            for (const auto& e2 : kb.entities) {
                std::unordered_map<std::string, std::string> varMap;
                bool conditionsMet = true;

                for (const auto& cond : rule.conditions) {
                    if (cond.find("~") != std::string::npos) {
                        std::string alias = extractAlias(cond);
                        std::string relType = state.aliases.at(alias);
                        std::string var1 = cond.substr(0, cond.find(' '));
                        std::string var2 = cond.substr(cond.rfind(' ') + 1);

                        bool found = false;
                        for (const auto& e : kb.entities) {
                            if (varMap.count(var1) && varMap.count(var2)) {
                                if (checkRelation(varMap[var1], varMap[var2], relType, rule.conditions)) {
                                    found = true;
                                    break;
                                }
                            } else if (varMap.count(var1)) {
                                if (checkRelation(varMap[var1], e.id, relType, rule.conditions)) {
                                    varMap[var2] = e.id;
                                    found = true;
                                    break;
                                }
                            } else if (varMap.count(var2)) {
                                if (checkRelation(e.id, varMap[var2], relType, rule.conditions)) {
                                    varMap[var1] = e.id;
                                    found = true;
                                    break;
                                }
                            } else {
                                if (checkRelation(e1.id, e2.id, relType, rule.conditions)) {
                                    varMap[var1] = e1.id;
                                    varMap[var2] = e2.id;
                                    found = true;
                                    break;
                                }
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

                if (conditionsMet) { // && varMap.count(rule.from) && varMap.count(rule.to)) {
                    std::cout << "Inferred: " << varMap[rule.from] << " -> " << varMap[rule.to]
                              << " : " << rule.relation << " {";
                    for (const auto& [k, v] : rule.relProperties) {
                        std::cout << k << "=\"" << v << "\" ";
                    }
                    std::cout << "}\n";

                    kb.addRelation(varMap[rule.from], varMap[rule.to],
                                   state.aliases.count(rule.relation) ? state.aliases.at(rule.relation) : rule.relation,
                                   rule.relProperties);
                } else {
                    std::cout << "conditions didn't meet rule " << rule.name << std::endl;
                }
            }
        }
    }

    bool checkRelation(const std::string& from, const std::string& to, const std::string& type,
                       const std::vector<std::string>& conditions) {
        auto it = std::find_if(kb.relations.begin(), kb.relations.end(),
                               [&](const Relation& r) { return r.from == from && r.to == to && r.type == type; });
        if (it == kb.relations.end()) return false;

        for (const auto& cond : conditions) {
            if (cond.find('=') != std::string::npos && cond.find("~") == std::string::npos) {
                std::string key = cond.substr(0, cond.find('='));
                std::string value = cond.substr(cond.find('=') + 2, cond.length() - cond.find('=') - 3);
                auto propIt = it->properties.find(key);
                if (propIt == it->properties.end() || propIt->second != value) {
                    return false;
                }
            }
        }
        return true;
    }

    std::string getAliasOrType(const std::string& type) {
        for (const auto& [alias, fullType] : state.aliases) {
            if (fullType == type) return alias;
        }
        return type;
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
