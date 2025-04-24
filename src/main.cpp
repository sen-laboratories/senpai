// main.cpp
#include <iostream>
#include "inference_engine.h"

int main() {
    std::string dsl = R"dsl(
        USE relation/family-link AS genealogy
        USE relation/book-quote AS quotation
        USE relation/locality AS locality

        CONTEXT text/* {
            RULE quoted_by {
                IF (A ~quotation B)
                THEN RELATE(B, A, "quotation") WITH type="inverse", label="quoted by"
            }
        }
        CONTEXT application/person {
            RULE father_of {
                IF (A ~genealogy B AND role="parent of" AND A HAS gender="male")
                THEN RELATE(A, B, "genealogy") WITH label="father of"
            }
            RULE child_of {
                IF (A ~genealogy B AND role="parent of")
                THEN RELATE(B, A, "genealogy") WITH label="child of"
            }
            RULE son_of {
                IF (A ~genealogy B AND role="parent of" AND B HAS gender="male")
                THEN RELATE(B, A, "genealogy") WITH label="son of"
            }
            RULE daughter_of {
                IF (A ~genealogy B AND role="parent of" AND B HAS gender="female")
                THEN RELATE(A, B, "genealogy") WITH label="daughter of"
            }
        }
        CONTEXT entity/place {
            RULE contained_location {
                IF (A ~locality B AND role="located in" AND B ~locality C AND role="located in")
                THEN RELATE(A, C, "locality") WITH role="located in")
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

    // family example
    engine.add_predicate("John", "gender", "male");
    engine.add_predicate("Mary", "gender", "female");
    engine.add_predicate("Bob", "gender", "male");
    engine.add_fact("genealogy", "John", "Mary", {{"role", "parent of"}});
    engine.add_fact("genealogy", "Mary", "Bob", {{"role", "parent of"}});

    // quotation example
    engine.add_fact("quotes", "Book1", "Quote1", {});

    // locality example
    engine.add_fact("locality", "Vienna", "Austria", {{"role", "located in"}});
    engine.add_fact("locality", "Austria", "Europe", {{"role", "located in"}});

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