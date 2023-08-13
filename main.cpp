#include <iostream>
#include "httpServer.h"

int main(int argc, const char** argv)
{
    Ys::httpServer::instance()->start("0.0.0.0:36613");

    return 0;
}