'use strict'
/**
 * \brief Ukázkový skript pro první projekt předmětu WAP
 */

/// Využijeme knihovnu, která je předmětem zadání projektu
import { iterateProperties } from "./iterate.mjs";

// Pomocná funkce pro tuto ukázku
function logGeneratedValues(gen) {
    for (let prop of gen) {
        console.log(prop);
    }
}

/// knihovna musí obsahovat generátor iterateProperties. Ukázka jeho použití
/// (bez popisovačů vlastností):
console.log(">>>>>>>>>> Ukázka 1");
/// Vyzkoušíme Object.prototype
logGeneratedValues(iterateProperties(Object.prototype));

/// a s vlasními objekty
console.log(">>>>>>>>>> Ukázka 2"); // Několik vlastností, mj. toString and valueOf
let o = {
    a: 1,
    b: 2,
    c: 3
}
logGeneratedValues(iterateProperties(o)) // (vlastnosti Object.prototype) a b c

console.log(">>>>>>>>>> Ukázka 3");
let p = Object.create(o);
Object.defineProperty(p, "a", {
    value: 10,
    writable: false,
    enumerable: true,
});

logGeneratedValues(iterateProperties(p)) // (vlastnosti Object.prototype) a b c a

/// Použití s filtrací podle popisovačů vlastností
console.log(">>>>>>>>>> Ukázka 4");
logGeneratedValues(iterateProperties(p, { enumerable: true })) // a b c a
console.log(">>>>>>>>>> Ukázka 5");
logGeneratedValues(iterateProperties(p, { writable: false })) // a

/// Iterátorů  vrácených generátorem je možné vytvořit více, vzájemně budou na
/// sobě nezávislé. Předpokládejte, že v průběhu iterace nebudou objekty měněné
console.log(">>>>>>>>>> Ukázka 6 (mix)");
let it1 = iterateProperties(p, { enumerable: true });
console.log(it1.next().value); // a
console.log(it1.next().value); // b
let it2 = iterateProperties(p, { enumerable: true });
console.log(it2.next().value); // a
console.log(it1.next().value); // c
console.log(it2.next().value); // b
console.log(it2.next().value); // c

console.log(">>>>>>>>>> Ukázka 7 (konstruktor)");
function Point(x, y) {
    this.x = x;
    this.y = y;
}
Point.prototype = {
    x: undefined,
    y: undefined
}
logGeneratedValues(iterateProperties(new Point(1.5, 6.3), { enumerable: true })) // x y x y

console.log(">>>>>>>>>> Ukázka 8 (třídy)");
class Point3D {
    x = undefined;
    y = undefined;
    z = undefined;
    navíc = undefined;
    constructor(x, y, z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }
}
logGeneratedValues(iterateProperties(new Point3D(1.5, 6.3, 5), { enumerable: true })) // x y z navíc
