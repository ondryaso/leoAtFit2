/**
 * @file Provides an {@link iterateProperties} generator that yields names of properties
 * in a given object and its prototype chain.
 * @module iterate
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 */

'use strict';

/**
 * Returns a generator that yields names of properties in a given object and its prototype chain.
 * For properties identified by a {@link Symbol}, the string representation of the identifier is yielded.
 * 
 * If {@link filterDescriptor} is provided, its defined properties are used as filters that control
 * which properties are returned. Their values are matched against the corresponding values in each property
 * descriptor in the target object chain.
 * 
 * The generator doesn't guarantee any particular order of the returned properties.
 * In this implementation, the names are yielded in order of {@link Reflect.ownKeys}, starting with the provided
 * source object and going up its prototype chain.
 * 
 * @param {*} inObj The object to iterate properties of.
 * @param {PropertyDescriptor?} filterDescriptor A descriptor-like object that specify values to filter by.
 * Only the `configurable`, `enumerable`, `value` and `writable` properties are used as filters (if defined). 
 * Any other properties are ignored. If undefined or null, no filtering is performed.
 * @yields {string} The next property name.
 * @returns {Generator<string, any, unknown>} A generator that yields string property names.
 * @example
 * // Returns an iterable of "a", "constructor", "__defineGetter__", etc.
 * // (all properties from Object.prototype are included as well)
 * iterateProperties({ a: "val" })
 * @example
 * // Returns an iterable of "a", "c"
 * iterateProperties({ a: "val", b: "val2", c: "val" }, { value: "val" })
 * @example
 * // Returns "a", "b"
 * iterateProperties({ a: undefined, b: null })
 * // Returns "a"
 * iterateProperties({ a: undefined, b: null }, { value: undefined })
 */
export function* iterateProperties(inObj, filterDescriptor = undefined) {
    let currentObj = inObj;

    while (currentObj !== null) {
        let keys = Reflect.ownKeys(currentObj);

        for (let key of keys) {
            if (filterDescriptor && !matchesDescriptor(Object.getOwnPropertyDescriptor(currentObj, key),
                filterDescriptor))
                continue;

            yield key.toString();
        }

        currentObj = Object.getPrototypeOf(currentObj);
    }
}

/**
 * Determines whether a provided property descriptor matches a filter.
 * A descriptor matches a filter if values of all the defined fields of the filter equal
 * the values in the source descriptor. Undefined fields in the filter are ignored.
 * 
 * @param {PropertyDescriptor} target The property descriptor to match.
 * @param {PropertyDescriptor} filter The filter property descriptor to match against.
 */
function matchesDescriptor(target, filter) {
    // 'in' must be used here because we want to allow filtering by undefined _value_,
    // that is, filter = { value: undefined }
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
