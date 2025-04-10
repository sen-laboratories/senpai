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

#include <String.h>

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

// Grammar rules accepting liberal whitespace use, line comments and is case insensitive
struct line_comment : seq<two<'/'>, until<eolf>> {};
struct ws : sor<space/*, line_comment*/> {};
struct wss : star<ws> {};
struct wsp : plus<ws> {};
struct wsb : star<sor<wsp, eol>> {};

struct Special : one<'/', '-', '_', '.', '*'> {};
struct Identifier : plus<sor<alnum, Special>> {};
struct String : seq<one<'"'>, plus<not_at<one<'"'>>, any>, one<'"'>> {};
struct TypeName : Identifier {};

struct PropertyPair : seq<Identifier, one<'='>, String> {};
struct HasClause : seq<Identifier, wsp, istring<'h', 'a', 's'>, wsp, PropertyPair, star<seq<one<','>, wsp, PropertyPair>>> {};
struct TildeRelation : seq<Identifier, wsp, one<'~'>, Identifier, wsp, Identifier> {};
struct PropertyCheck : seq<Identifier, one<'='>, String> {};
struct Condition : sor<HasClause, TildeRelation> {};
struct AndClause : seq<plus<ws>, istring<'A', 'N', 'D'>, wsp, sor<Condition, PropertyCheck>> {};
struct IfClause : seq<istring<'i', 'f'>, plus<ws>, one<'('>, wss, Condition, star<AndClause>, wss, one<')'>> {};
struct WithClause : seq<plus<ws>, istring<'W', 'I', 'T', 'H'>, plus<ws>, PropertyPair, star<seq<one<','>, plus<ws>, PropertyPair>>> {};
struct ThenClause : seq<istring<'t', 'h', 'e', 'n'>, wsp, istring<'r', 'e', 'l', 'a', 't', 'e'>, wss, one<'('>,
                       Identifier, star<ws>, one<','>, wss, Identifier, wss, one<','>, wss, String, wss, one<')'>,
                       opt<WithClause>> {};

struct UseDecl : seq<istring<'u', 's', 'e'>, wsp, TypeName, wsp, istring<'A', 'S'>, wsp, Identifier> {};
struct Rule : seq<wsb, istring<'r', 'u', 'l', 'e'>, wsp, Identifier, wsp, one<'{'>, wsb, IfClause, wsb, ThenClause, wsb, one<'}'>, wsb> {};
struct ContextStart : seq<istring<'c', 'o', 'n', 't', 'e', 'x', 't'>, wsp, Identifier> {};
struct ContextDef : seq<ContextStart, wsp, one<'{'>, wsb, plus<Rule>, wsb, one<'}'>> {};
struct RuleOrContext : sor<ContextDef, Rule> {};

struct Statement : sor<UseDecl, RuleOrContext> {};
struct StatementList : plus<Statement, wsb> {};
struct Grammar : seq<wsb, StatementList, eof> {};

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
    // for e.g. RULE definitions, we need to buffer nested conditions while the rule is still being parsed
    // e.g. RULE ... IF ... THEN
    RuleDef pendingRule;
};

template<typename Rule> struct Action : nothing<Rule> {};

template<> struct Action<UseDecl> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t asPos = s.find(" AS ");
        std::string type  = s.substr(4, asPos - 4);
        std::string alias = s.substr(asPos + 4);
        std::cout << "USE alias '" << alias << "' for type '" << type << "'" << std::endl;
        state.aliases[alias] = type;
        state.currentBlock = "use " + alias;
    }
};

template<> struct Action<ContextStart> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t idStart = s.find_first_not_of(" \t\n\r", 7);  // After "context"
        size_t idEnd = s.length();  // Until end of match
        std::string mimeType = BString(s.substr(idStart, idEnd - idStart).c_str()).Trim().RemoveAll("\"").String();

        std::cout << "starting context '" << mimeType << "'\n";
        state.contexts.push_back({mimeType, {}});
        state.currentBlock = "context " + mimeType;
    }
};

template<> struct Action<Rule> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t nameStart = s.find_first_not_of(" \t\n\r", 4);  // After "rule"
        size_t nameEnd = s.find(' ', nameStart);

        state.pendingRule.name = s.substr(nameStart, nameEnd - nameStart);

        // associate with correct context
        if (state.contexts.empty()) {
            state.contexts.push_back({"", {}});
        }
        std::string currentContext = state.contexts.back().mimeType;
        std::cout << "building rule '" << state.pendingRule.name << "' with context '" << currentContext << "'\n";

        state.contexts.back().rules.push_back(state.pendingRule);

        std::cout << "added data from pending rule: from='" << state.pendingRule.from << "', to='" << state.pendingRule.to
                  << "', relation='" << state.pendingRule.relation << "', with "
                  << state.pendingRule.relProperties.size() << " properties"
                  << " to context " << currentContext << std::endl;

        state.currentBlock = "rule " + state.pendingRule.name;
        state.pendingRule = RuleDef();
    }
};

template<> struct Action<HasClause> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();
        size_t hasPos   = s.find("has", 0);
        size_t eqPos    = s.find("=", hasPos);
        std::string key = BString(s.substr(hasPos + 4, eqPos - hasPos - 3 - 1).c_str()).Trim().String();
        std::string value = BString(s.substr(eqPos + 1).c_str()).Trim().RemoveAll("\"").String();

        std::cout << "   HAS '" << key << "' = '" << value << "'" << std::endl;
        state.pendingRule.conditions.push_back(key + " has " + value);
    }
};

template<> struct Action<TildeRelation> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        state.pendingRule.conditions.push_back(in.string());
    }
};

template<> struct Action<PropertyCheck> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        state.pendingRule.conditions.push_back(in.string());
    }
};

template<> struct Action<ThenClause> {
    template<typename ActionInput>
    static void apply(const ActionInput& in, State& state) {
        std::string s = in.string();

        // e.g.: ' then relate(B, A, "quoted-by")   '
        size_t bracketOpen = s.find('(');
        size_t firstComma = s.find(',');
        size_t secondComma = s.find(',', firstComma + 1);
        size_t bracketClose = s.find(')', secondComma + 1);

        auto& rule = state.pendingRule;
        rule.from = BString(s.substr(bracketOpen + 1, firstComma - bracketOpen - 1).c_str()).Trim().String();
        rule.to = BString(s.substr(firstComma + 1, secondComma - firstComma - 1).c_str()).Trim().String();
        rule.relation = BString(s.substr(secondComma + 1, bracketClose - secondComma - 1).c_str()).Trim()
                              .RemoveAll("\"").String();

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
                std::cout << "Context: '" << ctx.mimeType << "', Rules: " << ctx.rules.size() << std::endl;
            }
        } catch (const parse_error& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
            throw;
        }
    }

    void loadFromSource(const std::string& sourceId) {
        std::unordered_set<std::string> loadedIds;
        loadEntityAndRelations(sourceId, 0, loadedIds);
        std::cout << "Loaded " << kb.relations.size() << " relations" << std::endl;
    }

    void infer(const std::string& sourceMimeType) {
        std::cout << "infer from type " << sourceMimeType << std::endl;
        std::cout << "contexts:" << std::endl;
        for (const auto& context : state.contexts) {
            std::cout << context.mimeType << std::endl;
        }

        std::cout << std::endl << "*** Running inference engine..." << std::endl << std::endl;

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
            {"id3", {{"name", "Alice"}, {"gender", "female"}, {"mime", "application/person"}}},
            {"id4", {{"name", "Bob"}, {"gender", "male"}, {"mime", "application/person"}}},
            {"id5", {{"name", "Charlie"}, {"gender", "male"}, {"mime", "application/person"}}}
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
            {"id3id4", {{"role", "parent of"}}},
            {"id4id5", {{"role", "parent of"}}}
        };
        return dummyProps[fromId + toId];
    }

    void applyRule(const RuleDef& rule) {
        std::cout << "evaluating rule: '" << rule.name << "'\n";
        bool isTransitive = rule.name == "transitive";  // todo: implement a more advanced check later;-)
        for (const auto& e1 : kb.entities) {
            for (const auto& e2 : kb.entities) {
                std::unordered_map<std::string, std::string> varMap;
                bool conditionsMet = true;

                if (isTransitive) {
                    for (const auto& e3 : kb.entities) {
                        for (const auto& e4 : kb.entities) {
                            conditionsMet = checkTransitive(e1.id, e2.id, e3.id, e4.id, rule.conditions);
                            if (conditionsMet) {
                                varMap["A"] = e1.id;
                                varMap["B"] = e3.id;
                                varMap["C"] = e4.id;
                                varMap["D"] = e2.id;
                                break;
                            }
                        }
                        if (conditionsMet) break;
                    }
                } else {
                    for (const auto& cond : rule.conditions) {
                        if (cond.find("~") != std::string::npos) {
                            std::string alias = extractAlias(cond);
                            std::string relType = state.aliases.at(alias);
                            std::string var1 = cond.substr(0, cond.find(' '));
                            std::string var2 = cond.substr(cond.rfind(' ') + 1);
                            if (checkRelation(e1.id, e2.id, relType, rule.conditions)) {
                                varMap[var1] = e1.id;
                                varMap[var2] = e2.id;
                            } else {
                                conditionsMet = false;
                            }
                        } else if (cond.find("has") != std::string::npos) {
                            std::string var = cond.substr(0, cond.find(" "));
                            std::string props = cond.substr(cond.find("has") + 4);
                            size_t pos = 0;
                            auto entityProps = queryEntityAttributes(e1.id);
                            while (pos < props.length()) {
                                size_t eqPos = props.find('=', pos);
                                if (eqPos == std::string::npos) break;

                                std::string key = props.substr(pos, eqPos - pos);
                                size_t quoteStart = eqPos + 1;
                                size_t quoteEnd = props.find('"', quoteStart + 1);

                                std::string value = props.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

                                if (entityProps[key] != value) {
                                    conditionsMet = false;
                                    break;
                                }

                                pos = quoteEnd + 1;
                                if (pos < props.length() && props[pos] == ',') pos += 2;  // Skip comma and space
                            }
                            if (conditionsMet) varMap[var] = e1.id;
                        }
                        if (!conditionsMet) break;
                    }
                }

                if (conditionsMet && varMap.count(rule.from) && varMap.count(rule.to)) {
                    std::cout << "* Inferred: " << varMap[rule.from] << " -> " << varMap[rule.to]
                              << " : " << rule.relation << " {";
                    for (const auto& [k, v] : rule.relProperties) {
                        std::cout << k << "=\"" << v << "\" ";
                    }
                    std::cout << "}\n";

                    kb.addRelation(varMap[rule.from], varMap[rule.to],
                                   state.aliases.count(rule.relation) ? state.aliases.at(rule.relation) : rule.relation,
                                   rule.relProperties);
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
                std::cout << "  check condition " << cond << "\n";
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

    bool checkTransitive(const std::string& a, const std::string& d, const std::string& b, const std::string& c,
                         const std::vector<std::string>& conditions) {
        std::string relType;

        for (const auto& cond : conditions) {
            if (cond.find("~") != std::string::npos) {
                relType = state.aliases.at(extractAlias(cond));
                break;
            }
        }

        bool abMatch = checkRelation(a, b, relType, conditions);
        bool bcMatch = checkRelation(b, c, relType, conditions);
        bool cdMatch = checkRelation(c, d, relType, conditions);

        if (abMatch && bcMatch && cdMatch) {
            std::cout << "    Matched " << a << " -> " << b << " -> " << c << " -> " << d << "\n";
            return true;
        }

        return false;
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
