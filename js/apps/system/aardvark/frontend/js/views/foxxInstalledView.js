/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, EJS, $, window, _, templateEngine*/

(function() {
  "use strict";
  window.FoxxInstalledView = Backbone.View.extend({
    tagName: 'li',
    className: "tile",
    template: templateEngine.createTemplate("foxxInstalledView.ejs"),

    events: {
      'click .install': 'installFoxx'
    },

    initialize: function(){
      _.bindAll(this, 'render');
    },

    installFoxx: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "application/available/" + encodeURIComponent(this.model.get("_key")),
        {
          trigger: true
        }
      );
    },

    render: function(){
      $(this.el).html(this.template.render(this.model));
      return $(this.el);
    }
  });
}());
