#ifndef UTIL_H
#define UTIL_H

int randint(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

#endif //UTIL_H
