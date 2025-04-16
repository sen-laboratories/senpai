#include "inference_engine.h"

InferenceEngine::InferenceEngine(KnowledgeBase& kb, int depth) : kb(kb), maxDepth(depth) {}

void InferenceEngine::loadDSL(const std::string& dsl) {
    try {
        size_t problems = analyze<grammar>();
        if (problems > 0) {
            std::cout << "grammar analysis found " << problems << " problems, aborting." << std::endl;
            exit(1);
        }

        //standard_trace<grammar>(string_input(dsl, "DSL"), state);
        parse<grammar, Action>(string_input(dsl, "DSL"), state);

        std::cout << "Parsed " << state.contexts.size() << " contexts\n";

        for (const auto& ctx : state.contexts) {
            std::cout << "Context: '" << ctx.mimeType << "', Rules: " << ctx.rules.size() << std::endl;
        }
    } catch (const parse_error& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        throw;
    }
}

void InferenceEngine::loadFromSource(const std::string& sourceId) {
    // Assuming KnowledgeBase has a way to load data by ID
    // This is a stub—replace with your actual logic
    std::cout << "Loading data from source ID: " << sourceId << "\n";
    if (sourceId == "id1") {
        // Example: Load book data
        kb.entities.push_back({"book1", {{"title", "Book One"}}});
        kb.entities.push_back({"book2", {{"title", "Book Two"}}});
        kb.relations.push_back({"book1", "book2", "relation/book-quote", {{"quote", "Hello"}}});
    } else if (sourceId == "id3") {
        // Example: Load person data
        kb.entities.push_back({"person1", {{"gender", "male"}}});
        kb.entities.push_back({"person2", {{"gender", "female"}}});
        kb.relations.push_back({"person1", "person2", "relation/family-link", {{"role", "parent"}}});
    }
    std::cout << "Loaded " << kb.entities.size() << " entities and " << kb.relations.size() << " relations\n";
}

void InferenceEngine::infer(const std::string& sourceMime) {
    std::cout << "* infer from type " << sourceMime << "\n\n";
    for (const auto& context : state.contexts) {
        for (const auto& rule : context.rules) {
            applyRule(rule);
        }
    }
}

void InferenceEngine::applyRule(const RuleData& rule) {
    std::cout << "* evaluating rule: '" << rule.name << "'\n";
    bool isTransitive = rule.name == "transitive";
    for (const auto& e1 : kb.entities) {
        for (const auto& e2 : kb.entities) {
            std::unordered_map<std::string, std::string> varMap;
            bool conditionsMet = true;

            if (isTransitive) {
                for (const auto& e3 : kb.entities) {
                    conditionsMet = checkTransitive(e1.id, e2.id, e3.id, rule.conditions, varMap);
                    if (conditionsMet) {
                        varMap["A"] = e1.id;
                        varMap["B"] = e2.id;
                        varMap["C"] = e3.id;
                        break;
                    }
                }
            } else {
                for (const auto& cond : rule.conditions) {
                    if (cond.type == "relation") {
                        std::string relType = state.aliases.count(cond.relation) ? state.aliases.at(cond.relation) : cond.relation;
                        std::cout << "checking relation " << relType << " between " << e1.id << " and " << e2.id << "\n";
                        if (checkRelation(e1.id, e2.id, relType, rule.conditions)) {
                            varMap[cond.vars[0]] = e1.id;
                            varMap[cond.vars[1]] = e2.id;
                        } else {
                            std::cout << "    no match\n";
                            conditionsMet = false;
                            break;
                        }
                    } else if (cond.type == "has") {
                        std::string entityId = varMap[cond.vars[0]];
                        auto entityProps = queryEntityAttributes(entityId);
                        for (const auto& [key, value] : cond.properties) {
                            std::cout << "   HAS '" << key << "' = '" << value << "' on " << entityId << "\n";
                            if (entityProps.find(key) == entityProps.end() || entityProps[key] != value) {
                                std::cout << "    HAS condition failed\n";
                                conditionsMet = false;
                                break;
                            }
                        }
                        if (!conditionsMet) break;
                    }
                }
            }

            if (conditionsMet && varMap.count(rule.from) && varMap.count(rule.to)) {
                std::cout << "  * Inferred: " << varMap[rule.from] << " -> " << varMap[rule.to]
                          << " : " << rule.relation << " {";
                for (const auto& [k, v] : rule.relProperties) {
                    std::cout << k << "=\"" << v << "\" ";
                }
                std::cout << "}\n";
                kb.addRelation(varMap[rule.from], varMap[rule.to],
                               state.aliases.count(rule.relation) ? state.aliases.at(rule.relation) : rule.relation,
                               rule.relProperties);
            } else if (!conditionsMet) {
                std::cout << "  X conditions not met for " << e1.id << " -> " << e2.id << "\n";
            }
        }
    }
}

bool InferenceEngine::checkTransitive(const std::string& e1, const std::string& e2, const std::string& e3,
                                      const std::vector<ConditionData>& conditions,
                                      std::unordered_map<std::string, std::string>& varMap)
{
    bool conditionsMet = true;
    int relCount = 0;
    for (const auto& cond : conditions) {
        if (cond.type == "relation") {
            std::string relType = state.aliases.at(cond.relation);
            if (relCount == 0) {
                if (checkRelation(e1, e2, relType, conditions)) {
                    varMap[cond.vars[0]] = e1;
                    varMap[cond.vars[1]] = e2;
                } else {
                    conditionsMet = false;
                }
                relCount++;
            } else if (relCount == 1) {
                if (checkRelation(e2, e3, relType, conditions)) {
                    varMap[cond.vars[0]] = e2;
                    varMap[cond.vars[1]] = e3;
                } else {
                    conditionsMet = false;
                }
                relCount++;
            }
        }
    }

    if (conditionsMet) {
        std::cout << "    transitive match " << e1 << " -> " << e2 << " -> " << e3 << "\n";
    }

    return conditionsMet;
}

bool InferenceEngine::checkRelation(const std::string& from, const std::string& to, const std::string& type,
                                    const std::vector<ConditionData>& conditions)
{
    auto it = std::find_if(kb.relations.begin(), kb.relations.end(),
                           [&](const Relation& r) { return r.from == from && r.to == to && r.type == type; });
    if (it == kb.relations.end()) return false;

    for (const auto& cond : conditions) {
        if (cond.type == "property") {
            for (const auto& [key, value] : cond.properties) {
                std::string lowerKey = key;
                std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
                auto propIt = it->properties.find(lowerKey);
                std::cout << "  check condition '" << key << "' = '" << value << "': ";
                if (propIt == it->properties.end()) {
                    std::cout << "no property found\n";
                    return false;
                }
                std::string propValue = propIt->second;
                std::transform(propValue.begin(), propValue.end(), propValue.begin(), ::tolower);
                std::cout << (propValue == value ? "MATCHES" : "!=") << " '" << propIt->second << "'\n";
                if (propValue != value) return false;
            }
        }
    }

    return true;
}

std::unordered_map<std::string, std::string> InferenceEngine::queryEntityAttributes(const std::string& entityId) {
    // Assuming KnowledgeBase has a way to get entity attributes; stubbed for now
    std::unordered_map<std::string, std::string> props;
    for (const auto& entity : kb.entities) {
        if (entity.id == entityId) {
            props = entity.properties;
            break;
        }
    }
    return props;
}
