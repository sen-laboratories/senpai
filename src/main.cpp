#include <iostream>

#include "inference_engine.h"

int main() {
    KnowledgeBase kb;
    InferenceEngine engine(kb, 2);

    std::string dsl = R"(
        USE relation/book-quote AS quotes
        USE relation/family-link AS genealogy

        CONTEXT text/book {
            RULE quoted_by {
                IF (A ~quotes B)
                THEN RELATE(B, A, "quotes") WITH type="inverse", label="quoted by"
            }
        }

        CONTEXT application/person {
            RULE child_of {
                IF (A ~genealogy B AND role="parent of")
                THEN RELATE(B, A, "genealogy") WITH role="child of"
            }
            RULE father_of {
                IF (A ~genealogy B AND role="parent of" AND A has gender="male")
                THEN RELATE(A, B, "genealogy") WITH label="father of"
            }
            RULE mother_of {
                IF (A ~genealogy B AND role="parent of" AND A has gender="female")
                THEN RELATE(A, B, "genealogy") WITH label="mother of"
            }
            RULE daughter_of {
                IF (A ~genealogy B AND role="parent of" AND B has gender="female")
                THEN RELATE(B, A, "genealogy") WITH label="daughter of"
            }
            RULE father_of {
                IF (A ~genealogy B AND role="parent of" AND B has gender="male")
                THEN RELATE(B, A, "genealogy") WITH label="son of"
            }
        }

        CONTEXT */* {
            RULE transitive {
                IF (A ~genealogy B AND role="parent of" AND B ~genealogy C AND role="parent of")
                THEN RELATE(A, C, "genealogy") WITH role="grandparent"
            }
        }
    )";

    try {
        engine.loadDSL(dsl);
        engine.loadFromSource("id1");  // Load book data
        engine.loadFromSource("id3");  // Load person data

        engine.infer("text/book");
        engine.infer("application/person");

        for (const auto& rel : kb.relations) {
            std::cout << rel.from << " -> " << rel.to << " : " << rel.type
            << (rel.inferred ? " *" : "")
            << " {";
            for (const auto& [k, v] : rel.properties) {
                std::cout << k << "=\"" << v << "\" ";
            }
            std::cout << "}\n";
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
