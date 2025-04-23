// main.cpp
#include <iostream>
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
                IF (A ~genealogy B AND role="parent of" AND A HAS gender="male")
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

    engine.add_fact("genealogy", "John", "Mary", {{"role", "parent of"}});
    engine.add_fact("genealogy", "Mary", "Alice", {{"role", "parent of"}});
    engine.add_fact("quotes", "Book1", "Quote1", {});
    engine.add_predicate("John", "gender", "male");
    engine.add_predicate("Mary", "gender", "female");
    engine.add_predicate("Alice", "gender", "female");

    std::cout << "First run (context */*, max_depth=2, iterations=2):\n";
    auto new_relations = engine.infer("*/*", 2, 2);

    for (const auto& rel : new_relations) {
        std::string relation_name = rel.relation_name;
        std::string relation_from = rel.var1;
        std::string relation_to   = rel.var2;
        auto relation_attrs= rel.attributes;

        std::cout << "Creating SEN relation: " << relation_name << "("
                  << relation_from << " -> " << relation_to << ")";

        if (!relation_attrs.empty()) {
            std::cout << " WITH ";
            for (size_t i = 0; i < relation_attrs.size(); ++i) {
                std::cout << relation_attrs[i].key << "=\"" << relation_attrs[i].value << "\"";
                if (i < relation_attrs.size() - 1) std::cout << ", ";
            }
        }
        std::cout << "\n";
    }

    return 0;
}