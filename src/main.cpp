// FILE: main.cpp
#include "inference_engine.h"
#include <iostream>

int main() {
    KnowledgeBase kb;
    InferenceEngine engine(kb, 2);

    std::string dsl = R"(
        use relation/book-quote AS quotes
        use relation/family-link AS genealogy

        context text/book {
            rule quoted_by {
                if (A ~quotes B)
                then relate(B, A, "quotes") with type="inverse", label="quoted by"
            }
        }

        context application/person {
            rule child_of {
                if (A ~genealogy B AND role="parent of")
                then relate(B, A, "genealogy") WITH role="child of"
            }
            rule father_of {
                if (A ~genealogy B AND role="parent of" AND A has gender="male")
                then relate(A, B, "genealogy") WITH label="father of"
            }
            rule mother_of {
                if (A ~genealogy B AND role="parent of" AND A has gender="female")
                then relate(A, B, "genealogy") WITH label="mother of"
            }
            rule daughter_of {
                if (A ~genealogy B AND role="parent of" AND B has gender="female")
                then relate(B, A, "genealogy") WITH label="daughter of"
            }
            rule father_of {
                if (A ~genealogy B AND role="parent of" AND B has gender="male")
                then relate(B, A, "genealogy") WITH label="son of"
            }
        }

        rule transitive {
            if (A ~genealogy B AND role="parent of" AND B ~genealogy C AND role="parent of")
            then relate(A, C, "genealogy") WITH role="grandfather"
        }
    )";

    try {
        engine.loadDSL(dsl);
        engine.loadFromSource("id1");  // Load book data
        engine.loadFromSource("id3");  // Load person data

        std::string sourceMime = kb.entities[0].properties.at("mime");
        engine.infer(sourceMime);

        for (const auto& rel : kb.relations) {
            std::cout << rel.from << " -> " << rel.to << " : " << rel.type << " {";
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
