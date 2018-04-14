# PURPOSE

I'm open sourcing some of the C++ utility code I've written for my game projects in the hopes that others will find them useful, and also possibly find some bugs in them before I do, or add features, or clean up some of the stuff I neglected, or whatever. Feel free to help out!

These utilities are C++11 (or newer, I'm not too strict about that), and meant to not need any wrapper code to use in my personal projects. They are all designed to fit a specific need of mine and aren't meant to be general purpose. 

# GON

GON ("Glaiel Object Notation") is "json without the crap", meant to be a very easy to edit text-based file format that represents structured data. XML is too verbose and JSON has too much unnecessary syntax (quotes around everything, equal signs and commas everywhere), so GON is basically a json parser that ignores any symbols that it doesn't NEED for parsing. You *should* be able to throw a normal json file at it and have it parse, but I haven't tested that extensively. The file gets loaded into a c++ structure that makes accessing the data from it pretty easy.

The only basic data type GON loads are strings. The c++ interface has shortcuts for converting those strings to ints, doubles, and bools on-demand instead of when loading. All entries in a GON object are a key followed by its data. The key must be a string, the data can either be a string, an array (of data), or another GON object. Strings do not need quotations marks around them unless you want them to include whitespace. Commas and equals signs are optional.

# Example 1
A list of widget counts. This is a list of 4 widgets and their values. This is a valid GON file, and its data memebers can be accessed in c++ like so: my_gonobject["whirly_widgets"].Number();  etc
```
    whirly_widgets 10
    twirly_widgets 15
    girly_widgets 4
    burly_widgets 1
```
    
# Example 2 
A list of factories that have widget counts. Its data memebers can be accessed in c++ like so: my_gonobject["big_factory"]["whirly_widgets"].Number();  etc
```
    big_factory {
        location "New York City"
    
        whirly_widgets 8346
        twirly_widgets 854687
        girly_widgets 44336
        burly_widgets 2673
    }
    
    little_factory {
        location "My Basement"
    
        whirly_widgets 10
        twirly_widgets 15
        girly_widgets 4
        burly_widgets 1
    }
 ```   
    
# Example 3
An array of weekdays. Note that no quotes or commas are needed, though you can add them in if it makes it more clear for you. Its data members can be accessed in c++ like so: my_gonobject["weekdays"].Length(), my_gonobject["weekdays"][0].String();  etc
```
    weekdays [Monday Tuesday Wednesday Thursday Friday Saturday Sunday]
```    
    
