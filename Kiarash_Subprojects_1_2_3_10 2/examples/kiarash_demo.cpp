#include "persistence/PersistenceServices.h"
#include "startup/StartupServices.h"

#include <iostream>

int main()
{
    kiarash::ProjectCreationService creation;
    auto created = creation.create({"Demo Project", "A4", std::nullopt, std::nullopt});
    if(!created.ok())
    {
        std::cerr << created.message() << '\n';
        return 1;
    }

    kiarash::ProjectSerializer serializer;
    std::cout << serializer.serialize(created.value()) << '\n';
    return 0;
}
