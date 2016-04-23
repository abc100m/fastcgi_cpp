/*
* Copyright(c) 2016 dragon jiang<jianlinlong@gmail.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef _DYNAMIC_FACTORY_H_
#define _DYNAMIC_FACTORY_H_


#include <functional>
#include <map>
#include <memory>

//http://stackoverflow.com/questions/19036462/c-generic-object-factory-by-string-name
//https://gist.github.com/sacko87/3359911
//http://www.codeproject.com/Articles/567242/AplusC-b-bplusObjectplusFactory

template <typename Base_Type>
class Instance_Factory
{
public:
    typedef Base_Type Base;
    typedef std::function<Base*(void)> FN;

    static Instance_Factory* instance()
    {
        static Instance_Factory factory;
        return &factory;
    }

    /*
    * @name class name
    * @fn   a creator which create instance of @name
    */
    void register_function(const std::string& name, FN fn)
    {
        // register the class factory function
        name_fn_map_[name] = fn;
    }

    /*
    * create instance by class name
    * @name class name
    */
    std::shared_ptr<Base> create(const std::string& name)
    {
        Base * instance = nullptr;

        // find name in the registry and call factory method.
        auto it = name_fn_map_.find(name);
        if (it != name_fn_map_.end())
            instance = it->second();

        // wrap instance in a shared ptr and return
        if (instance != nullptr)
            return std::shared_ptr<Base>(instance);
        else
            return nullptr;
    }

private:
    std::map<std::string, FN> name_fn_map_;
};


template<typename T, typename Base_Type>
class Registrar
{
    typedef Base_Type Base;
public:
    Registrar(const std::string& class_name) 
    {
        // register the class factory function 
        Instance_Factory<Base>::instance()->register_function(class_name,
            [](void) -> Base * { return new T(); });
    }
};

/*
* @T         instance type, which is inherit from @Base_Type
* @Base_Type paren type of @T
*/
#define REGISTRAR_CLASS_2(T, Base_Type) \
    static Registrar<T, Base_Type> s___creator__##T(#T)


#if 0  //Demo Code -->create instance from string

#include "dynamic_factory.h"

int main(int argc, char *argv[])
{
    struct X {
    };

    struct A : public X{
        int a = 100;
    };
    struct B : public X{
        int b = 999;
    };


    REGISTRAR_CLASS_2(A, X);
    REGISTRAR_CLASS_2(B, X);
    auto x1 = Instance_Factory<X>::instance()->create("A");
    auto x2 = Instance_Factory<X>::instance()->create("B");
}

#endif

#endif
