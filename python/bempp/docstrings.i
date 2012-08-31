// Automatically specify argument lists and return types in docstrings
//%feature("autodoc", 0);

%define DECLARE_CLASS_DOCSTRING(class)
%feature("docstring", class ## _docstring) class
%enddef

// class -> class name
// method -> method name 
// auto_param_list -> if true, the parameter list and result's type will be 
// generated automatically, otherwise it should be provided explicitly 
// in the docstring
%define DECLARE_METHOD_DOCSTRING(class, method, auto_param_list)
#if auto_param_list
    %feature("autodoc", 0) class::method;
#endif
%feature("docstring", class ## _ ## method ## _docstring) class::method;
%enddef

%define DECLARE_TEMPLATE_CLASS_DOCSTRING(python_class, cpp_class)
%feature("docstring", python_class ## _docstring) cpp_class;
%enddef

%define DECLARE_TEMPLATE_METHOD_DOCSTRING(python_class, cpp_class, method, auto_param_list)
#if !auto_param_list
    %feature("autodoc", python_class ## _ ## method ## _autodoc_docstring) cpp_class::method;
#endif
%feature("docstring", python_class ## _ ## method ## _docstring) cpp_class::method;
%enddef

%define DECLARE_TEMPLATE_VALUE_METHOD_AUTO_DOCSTRING(class, method)
%feature("autodoc", 2) class<float>::method;
%feature("autodoc", 2) class<double>::method;
%feature("autodoc", 2) class<std::complex<float> >::method;
%feature("autodoc", 2) class<std::complex<double> >::method;
%enddef
