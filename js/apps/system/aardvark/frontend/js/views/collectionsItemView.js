/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, templateEngine*/

(function() {
  "use strict";

  window.CollectionListItemView = Backbone.View.extend({

    tagName: "li",
//    className: "span3",
    className: "tile",
    template: templateEngine.createTemplate("collectionsItemView.ejs"),

    initialize: function () {
      //this.model.bind("change", this.render, this);
      //this.model.bind("destroy", this.close, this);
    },
    events: {
      'click .pull-left' : 'noop',
      'click #editCollection' : 'editProperties',
      'click .spanInfo' : 'showProperties',
      'click': 'selectCollection'
    },
    render: function () {
      $(this.el).html(this.template.render(this.model));
      return this;
    },

    editProperties: function (event) {
      event.stopPropagation();
      window.App.navigate(
        "collection/" + encodeURIComponent(this.model.get("id")),
        {trigger: true}
      );
    },

    showProperties: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "collectionInfo/" + encodeURIComponent(this.model.get("id")), {trigger: true}
      );
    },
    
    selectCollection: function() {
      window.App.navigate(
        "collection/" + encodeURIComponent(this.model.get("name")) + "/documents/1", {trigger: true}
      );
    },
    
    noop: function(event) {
      event.stopPropagation();
    }

  });
}());
