#include <map>
#include <functional>
#include <iostream>
#include <cassert>

int main() {
    std::map<int, int, std::greater<int>> m;
    m[5] = 50;
    m[2] = 20;
    
    // Map order: 5, 2.
    
    // Test position 3
    auto it = m.lower_bound(3);
    
    if (it == m.end()) {
        std::cout << "lower_bound(3) returned end" << std::endl;
    } else {
        std::cout << "lower_bound(3) returned key: " << it->first << " val: " << it->second << std::endl;
    }
    
    // Test position 5
    it = m.lower_bound(5);
    std::cout << "lower_bound(5) returned key: " << it->first << std::endl;
    
    // Test position 6
    it = m.lower_bound(6);
    if (it == m.end()) { 
        std::cout << "lower_bound(6) returned end" << std::endl;
    } else {
         std::cout << "lower_bound(6) returned key: " << it->first << std::endl;
    }
    
    return 0;
}
