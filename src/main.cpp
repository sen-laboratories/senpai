// FILE: main.cpp
#include "inference_engine.h"
#include <iostream>

int main() {
    KnowledgeBase kb;
    InferenceEngine engine(kb, 2);
    std::string dsl = R"(
        use relation/book-quote AS quotes
        use relation/family-link AS father-of

        context text/book {
            rule quotes {
                if (A ~quotes B)
                then relate(A, B, "cites")
            }
        }

        rule grandfather {
            if (A ~father-of B and B ~father-of C)
            then relate(A, C, "grandfather")
        }
    )";

    try {
        engine.loadDSL(dsl);
        engine.loadFromSource("id1");  // Source entity "id1" (mime: text/book)
        std::string sourceMime = kb.entities[0].properties.at("mime");

        engine.infer(sourceMime);

        for (const auto& rel : kb.relations) {
            std::cout << rel.from << " -> " << rel.to << " : " << rel.type << "\n";
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
