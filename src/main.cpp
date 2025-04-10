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
                then relate(B, A, "quoted-by")
            }
        }

        context application/person {
            rule child_of {
                if (A ~genealogy B AND role="parent-of")
                then relate(B, A, "genealogy") WITH role="child-of"
            }
        }

        rule grandfather {
            if (A ~genealogy B AND role="parent-of" AND B ~genealogy C AND role="parent-of")
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
