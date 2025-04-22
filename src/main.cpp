// main.cpp
#include <iostream>
#include <optional>
#include "inference_engine.h"

int main() {
    std::string dsl = R"dsl(
        USE relation/family-link AS genealogy
        USE relation/book-quote AS quotes
        CONTEXT text/* {
            RULE quoted_by {
                IF (A ~quotes B)
                THEN RELATE(B, A, "quotes") WITH type="inverse", label="quoted by"
            }
        }
        CONTEXT application/person {
            RULE father_of {
                IF (A ~genealogy B AND role="parent of" AND A has gender="male")
                THEN RELATE(A, B, "genealogy") WITH label="father of"
            }
        }
        CONTEXT */* {
            RULE transitive {
                IF (A ~genealogy B AND role="parent of" AND B ~genealogy C AND role="parent of")
                THEN RELATE(A, C, "grand_parent_of") WITH role="grandparent"
            }
        }
    )dsl";

    sen::InferenceEngine engine;
    engine.parse(dsl);

    engine.add_fact({"genealogy", "John", "Mary", {{"role", "parent of"}}});
    engine.add_fact({"genealogy", "Mary", "Alice", {{"role", "parent of"}}});
    engine.add_fact({"quotes", "Book1", "Quote1", {}});
    engine.add_predicate({"John", "gender", "male"});
    engine.add_predicate({"Mary", "gender", "female"});
    engine.add_predicate({"Alice", "gender", "female"});

    std::cout << "First run (context */*, max_depth=2, iterations=2):\n";
    auto new_relations = engine.infer("*/*", 2, 2);
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

    std::cout << "\nSecond run (context application/person, max_depth=2, iterations=1):\n";
    new_relations = engine.infer("application/person", 2, 1);
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

    std::cout << "\nThird run (all text/* contexts, max_depth=2, iterations=2):\n";
    new_relations = engine.infer("text/*", 2, 2);
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

    return 0;
}