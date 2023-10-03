/**
 * @file Provides alternative implementations of the {@link module:iterate.iterateProperties} generator.
 * @module iterate_alternative
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @see {@link module:iterate} The baseline implementations. All implementations found
 * in this module follow the same contract.
 */

'use strict';

import { iterateProperties } from "./iterate.mjs";

/**
 * A recursive implementation that uses {@link Object.getOwnPropertyDescriptors}.
 * Retrieves an array of all descriptors of the object, iterates over it using {@link Reflect.ownKeys},
 * yields key-by-key and then uses yield* to call itself recursively for the prototype (if present).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_recursive_fetchDescriptors(inObj, matchDescriptor = undefined) {
    let ownPropertyDescriptors = Object.getOwnPropertyDescriptors(inObj);

    for (let property of Reflect.ownKeys(ownPropertyDescriptors)) {
        // @ts-ignore
        let descriptor = ownPropertyDescriptors[property];

        if (matchDescriptor && !matchesDescriptor(descriptor, matchDescriptor))
            continue;

        yield property.toString();
    }

    let proto = Object.getPrototypeOf(inObj);
    if (proto)
        yield* iterateProperties_recursive_fetchDescriptors(proto, matchDescriptor);
}

/**
 * A recursive implementation that uses {@link Reflect.ownKeys} directly on the source object, transforms it
 * using {@link Array.filter} and {@link Array.map} and uses yield* to return the result, followed by
 * a yield* to call itself recursively for the prototype (if present).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_recursive_functional(inObj, filterDescriptor = undefined) {
    let x = Reflect.ownKeys(inObj);

    if (filterDescriptor)
        x = x.filter(m => matchesDescriptor(Object.getOwnPropertyDescriptor(inObj, m), filterDescriptor));

    // Make symbols into strings
    x = x.map(m => m.toString());
    yield* x;

    let proto = Object.getPrototypeOf(inObj);
    if (proto)
        yield* iterateProperties_recursive_functional(proto, filterDescriptor);
}

/**
 * A recursive implementation that uses {@link Reflect.ownKeys} directly on the source object,
 * iterating over the resulting array it and yielding key-by-key, followed by
 * a yield* to call itself recursively for the prototype (if present).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_recursive_reflectOnInput(inObj, filterDescriptor = undefined) {
    let x = Reflect.ownKeys(inObj);

    for (let m of x) {
        if (filterDescriptor && !matchesDescriptor(Object.getOwnPropertyDescriptor(inObj, m), filterDescriptor))
            continue;

        yield m.toString();
    }

    let proto = Object.getPrototypeOf(inObj);
    if (proto)
        yield* iterateProperties_recursive_reflectOnInput(proto, filterDescriptor);
}

/**
 * An iterative implementation that uses {@link Object.getOwnPropertyDescriptors}.
 * Retrieves an array of all descriptors of the object and iterates over it using {@link Reflect.ownKeys}.
 * Collects all the property names of a single object in an array of results, uses yield* to return
 * them and moves up the prototype chain and repeats while a prototype exists (without using recursion).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_iterative_fetchDescriptors(inObj, matchDescriptor = undefined) {
    let currentObj = inObj;
    let result = [];

    while (currentObj !== null) {
        let ownPropertyDescriptors = Object.getOwnPropertyDescriptors(currentObj);

        for (let property of Reflect.ownKeys(ownPropertyDescriptors)) {
            // @ts-ignore
            let descriptor = ownPropertyDescriptors[property];

            if (matchDescriptor && !matchesDescriptor(descriptor, matchDescriptor))
                continue;

            result.push(property.toString());
        }

        yield* result;
        result = [];

        currentObj = Object.getPrototypeOf(currentObj);
    }
}

/**
 * An iterative implementation that uses {@link Reflect.ownKeys} directly on the source object, transforms it
 * using {@link Array.filter} and {@link Array.map} and uses yield* to return the result. Then it moves up
 * the prototype chain and repeats while a prototype exists (without using recursion).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_iterative_functional(inObj, filterDescriptor = undefined) {
    let currentObj = inObj;
    while (currentObj !== null) {
        let x = Reflect.ownKeys(currentObj);

        if (filterDescriptor)
            x = x.filter(m => matchesDescriptor(Object.getOwnPropertyDescriptor(currentObj, m), filterDescriptor));

        // Make symbols into strings
        x = x.map(m => m.toString());
        yield* x;

        currentObj = Object.getPrototypeOf(currentObj);
    }
}

/**
 * An iterative implementation that uses {@link Reflect.ownKeys} directly on the source object,
 * iterating over the resulting array it and yielding key-by-key. Then it moves up
 * the prototype chain and repeats while a prototype exists (without using recursion).
 * 
 * This only calls the baseline {@link module:iterate.iterateProperties} implementation.
 * This wrapper exists for the sake of consistency and naming in the performance tests logs which
 * uses {@link Function.name}.
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function iterateProperties_iterative_reflectOnInput(inObj, filterDescriptor) {
    return iterateProperties(inObj, filterDescriptor);
}

/**
 * An iterative implementation that uses {@link Object.getOwnPropertyNames} and
 * {@link Object.getOwnPropertySymbols} directly on the source object, iterating over
 * the resulting array it and yielding key-by-key. Then it moves up the prototype chain 
 * and repeats while a prototype exists (without using recursion).
 * 
 * @see {@link module:iterate.iterateProperties}
 */
export function* iterateProperties_iterative_objectMethods(inObj, filterDescriptor = undefined) {
    let currentObj = inObj;

    while (currentObj !== null) {
        let keys = Object.getOwnPropertyNames(currentObj);

        for (let key of keys) {
            if (filterDescriptor && !matchesDescriptor(Object.getOwnPropertyDescriptor(currentObj, key),
                filterDescriptor))
                continue;

            yield key;
        }

        let symbolKeys = Object.getOwnPropertySymbols(currentObj);

        for (let key of symbolKeys) {
            if (filterDescriptor && !matchesDescriptor(Object.getOwnPropertyDescriptor(currentObj, key),
                filterDescriptor))
                continue;

            yield key.toString();
        }

        currentObj = Object.getPrototypeOf(currentObj);
    }
}

/**
 * The same as `matchesDescriptor` in {@link module:iterate}.
 * Using that function directly would be preferred but exporting it would violate
 * the project specification.
 */
function matchesDescriptor(target, filter) {
    if ('value' in filter && target.value !== filter.value)
        return false;
    if ('configurable' in filter && target.configurable !== filter.configurable)
        return false;
    if ('enumerable' in filter && target.enumerable !== filter.enumerable)
        return false;
    if ('writable' in filter && target.writable !== filter.writable)
        return false;

    return true;
}